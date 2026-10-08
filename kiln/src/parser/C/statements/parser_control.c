#include <string.h>
#include "parser/H/core/parser.h"
#include "parser/H/core/parser_internal.h"
#include "parser/H/declarations/vars.h"
#include "parser/H/declarations/functions.h"
#include "parser/H/statements/each_loop.h"
#include "parser/H/statements/loop_stack.h"
#include "codegen/H/expressions/expr.h"
#include "codegen/H/emit/emit_sse.h"
#include "codegen/H/emit/value.h"

// Conditions may be parenthesized or not (`given (x < n) {` and
// `given x < n {` both work) -- unlike ashvm, there's no superinstruction
// fusion here to keep firing either way, so this is just a plain optional
// token, no caller-side terminator-passing needed.
static int consume_optional_lparen(void) {
    if (current.type != TOKEN_LPAREN) return 0;
    advance_token();
    return 1;
}

// The statement kinds involved enough to warrant their own file (mirrors
// ashvm/src/compiler/stmt_control.c's split from stmt.c for the same
// reason). Each function assumes statement()'s dispatch in parser.c
// already consumed the keyword token that identified it.

void given_statement(void) {
    advance_token();
    int parenthesized = consume_optional_lparen();
    codegen_expression();
    if (parenthesized) expect(TOKEN_RPAREN, "expected ')' after condition");
    codegen_pop_and_test_truthy();
    int otherwise_jump = emit_jcc_rel32(code, COND_E);

    block();
    if (current.type == TOKEN_OTHERWISE) {
        int end_jump = emit_jmp_rel32(code);
        emit_patch_jump(code, otherwise_jump);
        advance_token();
        block();
        emit_patch_jump(code, end_jump);
    } else {
        emit_patch_jump(code, otherwise_jump);
    }
}

void during_statement(void) {
    advance_token();
    int loop_start = code->count;
    int parenthesized = consume_optional_lparen();
    codegen_expression();
    if (parenthesized) expect(TOKEN_RPAREN, "expected ')' after condition");
    codegen_pop_and_test_truthy();
    int exit_jump = emit_jcc_rel32(code, COND_E);

    loop_push();
    block();
    loop_patch_nexts(); // next lands here, right before the loop-back
    emit_jmp_back(code, loop_start);
    emit_patch_jump(code, exit_jump);
    loop_pop_and_patch_stops(); // stop lands here, after the loop
}

// forge NAME ( [IDENT (, IDENT)*] ) block
//
// Function bodies are compiled inline into the SAME flat CodeBuf as
// top-level code (unlike ashvm, which gives each function its own Chunk)
// -- so declaring one emits an unconditional jump OVER the body first,
// making sure normal top-to-bottom execution never falls into a function
// it didn't explicitly call. The jump is patched once the body's end is
// known.
//
// Calling convention (kiln's own -- nothing external ever calls into
// generated code, so there's no ABI to match): caller pushes each
// argument's (tag, payload) pair -- 16 bytes, milestone 5 -- left to
// right via the normal expression codegen, no special reordering needed
// (see the offset derivation below), then `call`s the callee's
// already-known code offset (functions can only call ones declared
// earlier in the source, so the target is always known -- see
// emit_call_back). The standard push-rbp/mov-rbp,rsp/sub-rsp prologue
// establishes a fresh frame; the FIRST thing the body does is copy each
// argument's tag AND payload off the caller's pushed stack values into
// its own local slot, so afterward every variable access -- parameter or
// `local` -- goes through the exact same var_slot_*_offset() formulas.
// Because the last argument is the last one pushed (closest to the
// return address), and the first argument is deepest, parameter i's tag
// sits at [rbp + 16 + 16*(argc-1-i)] and its payload right after that
// (+16 skips the saved rbp and return address). Return value comes back
// as RBX=tag, RAX=payload (see parser.c's yield_statement).
void forge_statement(void) {
    advance_token();
    expect(TOKEN_IDENTIFIER, "expected function name after 'forge'");
    const char *name = previous.start;
    int name_len = previous.length;
    KilnFunction *fn = declare_function_in_context(name, name_len);

    expect(TOKEN_LPAREN, "expected '(' after function name");
    char param_names[MAX_KILN_PARAMS][64];
    int param_lens[MAX_KILN_PARAMS];
    int argc = 0;
    if (current.type != TOKEN_RPAREN) {
        for (;;) {
            expect(TOKEN_IDENTIFIER, "expected parameter name");
            if (argc >= MAX_KILN_PARAMS) parse_error("too many parameters");
            param_lens[argc] = previous.length;
            memcpy(param_names[argc], previous.start, (size_t)previous.length);
            argc++;
            if (current.type != TOKEN_COMMA) break;
            advance_token();
        }
    }
    expect(TOKEN_RPAREN, "expected ')' after parameters");
    fn->arity = argc;

    int skip_jump = emit_jmp_rel32(code);
    fn->code_offset = code->count;
    function_resolve_fixups(fn);

    VarScope outer_vars;
    vars_save(&outer_vars);
    vars_clear();
    int outer_each_depth;
    each_depth_save(&outer_each_depth);
    each_depth_reset();
    LoopScope outer_loop_depth;
    loop_scope_save(&outer_loop_depth);
    loop_scope_reset();

    emit_push_reg(code, REG_RBP);
    emit_mov_reg_reg(code, REG_RBP, REG_RSP);
    emit_sub_reg_imm32(code, REG_RSP, KILN_FRAME_RESERVE);

    for (int i = 0; i < argc; i++) {
        int slot = declare_var(param_names[i], param_lens[i]);
        int arg_base = 16 + 16 * (argc - 1 - i);
        // Within each argument's 16-byte block, payload sits at the LOWER
        // offset: the caller pushed tag-then-payload for each arg, and
        // since push grows the stack downward, whatever's pushed SECOND
        // (payload) ends up closer to rbp. (Everywhere else this project
        // pops in LIFO order, which gets this right automatically --
        // here the callee reads the caller's still-live stack region
        // directly instead of popping, so the offset had to be derived
        // by hand, and got it backwards on the first pass.)
        emit_load_mem_disp32(code, REG_RAX, REG_RBP, arg_base);
        emit_store_mem_disp32(code, REG_RBP, var_slot_payload_offset(slot), REG_RAX);
        emit_load_mem_disp32(code, REG_RBX, REG_RBP, arg_base + 8);
        emit_store_mem_disp32(code, REG_RBP, var_slot_tag_offset(slot), REG_RBX);
    }

    block();

    // Implicit `yield 0;` if the body falls through without one --
    // dead code on any path that already returned explicitly, harmless
    // (matches ashvm's compile_forge_decl, which does the same
    // unconditionally after compiling the body).
    emit_mov_reg_imm64(code, REG_RAX, 0); // 0.0's bits are all zero
    emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
    emit_mov_reg_reg(code, REG_RSP, REG_RBP);
    emit_pop_reg(code, REG_RBP);
    emit_ret(code);

    vars_restore(&outer_vars);
    each_depth_restore(outer_each_depth);
    loop_scope_restore(outer_loop_depth);
    emit_patch_jump(code, skip_jump);
}
