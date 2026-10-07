#include <stdlib.h>
#include <string.h>
#include "compiler/H/internal.h"
#include "diagnostics/H/diagnostics.h"
#include "builtins/H/builtins.h"
#include "compiler/H/loop_stack.h"

// Each function here assumes statement() already consumed the keyword token
// (TOKEN_FORGE / TOKEN_GIVEN / TOKEN_ATTEMPT / TOKEN_DURING) that identified it.

void compile_forge_decl(void) {
    expect(TOKEN_IDENTIFIER, "expected function name after 'forge'");
    const char *name = previous.start;
    int name_len = previous.length;
    expect(TOKEN_LPAREN, "expected '(' after function name");

    int fn_idx = declare_function_in_context(name, name_len);
    vm_functions[fn_idx].arity = 0;
    chunk_init(&vm_functions[fn_idx].chunk);

    char param_names[8][64];
    int param_lens[8];
    int argc = 0;
    if (current.type != TOKEN_RPAREN) {
        expect(TOKEN_IDENTIFIER, "expected parameter name");
        memcpy(param_names[argc], previous.start, previous.length);
        param_lens[argc] = previous.length;
        argc++;
        while (current.type == TOKEN_COMMA) {
            advance_token();
            expect(TOKEN_IDENTIFIER, "expected parameter name");
            memcpy(param_names[argc], previous.start, previous.length);
            param_lens[argc] = previous.length;
            argc++;
        }
    }
    expect(TOKEN_RPAREN, "expected ')' after parameters");
    vm_functions[fn_idx].arity = argc;

    Chunk *outer_chunk = chunk;
    char outer_locals[MAX_VM_LOCALS][64];
    int outer_local_count = local_count;
    memcpy(outer_locals, local_names, sizeof(local_names));
    int outer_loop_depth;
    loop_depth_save(&outer_loop_depth);
    loop_depth_reset();

    chunk = &vm_functions[fn_idx].chunk;
    local_count = 0;
    for (int i = 0; i < argc; i++) declare_local(param_names[i], param_lens[i]);

    block();
    emit_constant(vm_num(0));
    emit_op(OP_RETURN);

    chunk = outer_chunk;
    local_count = outer_local_count;
    memcpy(local_names, outer_locals, sizeof(local_names));
    loop_depth_restore(outer_loop_depth);
}

// Conditions may be parenthesized or not (`given (x < n) {` and
// `given x < n {` both work) -- try_fuse_condition() needs to know which,
// since the fused shape's lookahead has to stop at whatever actually
// follows the comparison: `)`, or straight on to the block's opening `{`.
static int consume_optional_lparen(void) {
    if (current.type != TOKEN_LPAREN) return 0;
    advance_token();
    return 1;
}

// After compiling a block that may be re-entered (a loop body) or whose
// sibling branch may take a different path (given/otherwise), resets the
// runtime stack back to exactly saved_local_count slots if the block
// declared any locals of its own, and restores compile-time bookkeeping
// to match. This is an ABSOLUTE reset (OP_TRUNC_LOCALS sets
// stack_top = frame->slots + n), not a fixed number of OP_POPs -- a jump
// that exits the block early (`next`, or one branch of a nested given)
// may have declared fewer locals than the block's "straight-line" path
// does, so only the known target depth is safe to assume. Without this,
// a fresh `local` declared inside a during/each body (or inside a given
// nested in one) would push one more stack slot every time the block
// re-runs, permanently stuck at slot `saved_local_count` from the first
// pass -- found via a cross-engine audit against kiln, which doesn't
// have this bug (its locals are fixed rbp-relative memory, not stack
// positions, so re-declaring one just overwrites the same address).
static void truncate_block_locals(int saved_local_count) {
    if (local_count != saved_local_count) {
        emit2(OP_TRUNC_LOCALS, (uint8_t)saved_local_count);
    }
    local_count = saved_local_count;
}

void compile_given_stmt(void) {
    int parenthesized = consume_optional_lparen();
    int patch_loc = try_fuse_condition(parenthesized ? TOKEN_RPAREN : TOKEN_LBRACE);
    int otherwise_jump;
    if (patch_loc != -1) {
        if (parenthesized) expect(TOKEN_RPAREN, "expected ')' after condition");
        otherwise_jump = patch_loc;
    } else {
        expression();
        if (parenthesized) expect(TOKEN_RPAREN, "expected ')' after condition");
        otherwise_jump = emit_jump(OP_JUMP_IF_FALSE);
    }
    int saved_local_count = local_count;
    block();
    truncate_block_locals(saved_local_count);
    if (current.type == TOKEN_OTHERWISE) {
        int end_jump = emit_jump(OP_JUMP);
        patch_jump(otherwise_jump);
        advance_token();
        block();
        truncate_block_locals(saved_local_count);
        patch_jump(end_jump);
    } else {
        patch_jump(otherwise_jump);
    }
}

void compile_attempt_stmt(void) {
    int saved_local_count = local_count;

    int attempt_jump = emit_jump(OP_TRY_PUSH);

    block();

    emit_op(OP_TRY_POP);
    int skip_handle = emit_jump(OP_JUMP);

    patch_jump(attempt_jump);

    expect(TOKEN_HANDLE, "expected 'handle' after attempt block");
    expect(TOKEN_LPAREN, "expected '(' after 'handle'");
    expect(TOKEN_IDENTIFIER, "expected error variable name");
    // 'e' must land at EXACTLY saved_local_count, not wherever local_count
    // ended up after compiling the attempt block -- since the attempt block
    // may have declared its own locals before erroring, but the runtime
    // error-value push always lands at the attempt-entry stack position,
    // regardless of what the attempt block attempted (all of that gets
    // discarded on the error path).
    memcpy(local_names[saved_local_count], previous.start, previous.length);
    local_names[saved_local_count][previous.length] = '\0';
    local_count = saved_local_count + 1;
    expect(TOKEN_RPAREN, "expected ')' after handle variable");

    block();
    emit_op(OP_POP);

    patch_jump(skip_handle);

    local_count = saved_local_count;
}

void compile_during_stmt(void) {
    int loop_start = chunk->count;
    int parenthesized = consume_optional_lparen();
    int patch_loc = try_fuse_condition(parenthesized ? TOKEN_RPAREN : TOKEN_LBRACE);
    int exit_jump;
    if (patch_loc != -1) {
        if (parenthesized) expect(TOKEN_RPAREN, "expected ')' after condition");
        exit_jump = patch_loc;
    } else {
        expression();
        if (parenthesized) expect(TOKEN_RPAREN, "expected ')' after condition");
        exit_jump = emit_jump(OP_JUMP_IF_FALSE);
    }
    int saved_local_count = local_count;
    loop_push();
    block();
    loop_patch_nexts(); // next lands here, right before the loop-back
    truncate_block_locals(saved_local_count);
    emit_loop(loop_start);
    patch_jump(exit_jump);
    loop_pop_and_patch_stops(); // stop lands here too -- same position as exit_jump
}

// each (x in arr) { body }  --  iterates an array by index, compiling down
// to exactly the bytecode a hand-written counting during-loop would produce
// (len(), OP_INDEX_GET, and OP_ACC_LOCAL for the index bump, wired together
// directly instead of going through expression()/statement() parsing of
// synthesized source text). Arrays only, matching map()/filter()/reduce()'s
// own array-only scope -- iterate a map via `each (k in keys(m))`.
//
// The three slots below (the array, the index, and the loop variable) are
// declared ONCE, before loop_start, and only ever updated via
// OP_SET_LOCAL/OP_ACC_LOCAL inside the repeated region -- never re-declared
// per iteration -- so the stack-slot layout stays stable across however
// many times the loop body actually runs (compiling a fresh declare_local()
// call into code that runs every iteration would desync the compile-time
// slot index from the runtime stack position after the first iteration).
// local_count is restored afterward (like compile_attempt_stmt does for its
// handle-variable) so repeated or nested `each` loops can't run the
// 128-slot MAX_VM_LOCALS table dry. The body's OWN locals (declared fresh
// inside `block()`, unlike these three setup slots) are handled separately
// by truncate_block_locals() every iteration -- see its comment.
void compile_each_stmt(void) {
    int saved_local_count = local_count;
    int parenthesized = consume_optional_lparen();

    expect(TOKEN_IDENTIFIER, "expected loop variable name after 'each'");
    const char *var_name = previous.start;
    int var_len = previous.length;
    expect(TOKEN_IN, "expected 'in' after loop variable name");

    expression(); // the collection expression; its value becomes the next local slot
    int arr_slot = declare_local("__each_arr", 10);

    emit_constant(vm_num(0));
    int idx_slot = declare_local("__each_idx", 10);

    emit_constant(vm_num(0)); // placeholder, overwritten before the body ever reads it
    int var_slot = declare_local(var_name, var_len);

    if (parenthesized) expect(TOKEN_RPAREN, "expected ')' after 'each' header");

    int len_id = vm_builtin_lookup("len", 3);

    int loop_start = chunk->count;
    emit2(OP_GET_LOCAL, (uint8_t)idx_slot);
    emit2(OP_GET_LOCAL, (uint8_t)arr_slot);
    emit_op(OP_CALL_BUILTIN);
    emit((uint8_t)len_id);
    emit((uint8_t)1);
    emit_op(OP_LT);
    int exit_jump = emit_jump(OP_JUMP_IF_FALSE);

    emit2(OP_GET_LOCAL, (uint8_t)arr_slot);
    emit2(OP_GET_LOCAL, (uint8_t)idx_slot);
    emit_op(OP_INDEX_GET);
    emit2(OP_SET_LOCAL, (uint8_t)var_slot);
    emit_op(OP_POP);

    int body_local_count = local_count; // past arr/idx/var, before the body's own locals
    loop_push();
    block();
    loop_patch_nexts(); // next lands here, right before the index bump
    truncate_block_locals(body_local_count);

    emit_op(OP_ACC_LOCAL);
    emit((uint8_t)idx_slot);
    emit((uint8_t)0x80); // op 0 ('+'), operand is the constant below
    emit((uint8_t)chunk_add_constant(chunk, vm_num(1)));

    emit_loop(loop_start);
    patch_jump(exit_jump);
    // stop must land at this EXACT bytecode position too (not after the
    // cleanup pops below) -- patch_jump() backfills an offset into
    // already-written bytes without advancing chunk->count, so this call
    // doesn't move "the current position" at all. Patching stop jumps to
    // land here means execution falls through into the same cleanup pops
    // below regardless of whether the loop ended via its own condition
    // going false or via an explicit `stop`.
    loop_pop_and_patch_stops();

    // local_count rolling back is compile-time bookkeeping only -- it does
    // NOT shrink the runtime stack, so the three slots pushed during setup
    // (arr, idx, var) are still physically sitting there when the loop
    // exits. Without these, a second `each` after this one would compile
    // its own slots to the same (now compile-time-reused) indices while
    // the runtime stack is still three deeper than that, desyncing every
    // GET_LOCAL/SET_LOCAL in it from where its values actually live.
    // compile_attempt_stmt's `emit_op(OP_POP);` after its handle-block
    // pops exactly this same kind of leftover for its one handle-variable
    // slot; this is the three-slot version of the same fix.
    emit_op(OP_POP);
    emit_op(OP_POP);
    emit_op(OP_POP);

    local_count = saved_local_count;
}
