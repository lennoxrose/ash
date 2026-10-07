#include <string.h>
#include "parser/parser.h"
#include "parser/parser_internal.h"
#include "parser/vars.h"
#include "parser/functions.h"
#include "parser/for_loop.h"
#include "parser/loop_stack.h"
#include "codegen/expr.h"
#include "codegen/emit_sse.h"
#include "codegen/value.h"

// Truthiness test for if/while conditions: pop the value (tag discarded,
// assumed NUMBER -- matches ashvm's own truthy(), which requires a
// number too) and compare its payload against 0.0 via ucomisd (not GP
// cmp -- see emit_sse.h). COND_E afterward means "was zero, i.e. falsy".
static void pop_and_test_truthy(void) {
    emit_pop_reg(code, REG_RAX); // payload
    emit_pop_reg(code, REG_RBX); // tag (assumed NUMBER)
    emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
    emit_pxor_xmm_xmm(code, XMM1); // XMM1 = +0.0
    emit_ucomisd(code, XMM0, XMM1);
}

// The statement kinds involved enough to warrant their own file (mirrors
// ashvm/src/compiler/stmt_control.c's split from stmt.c for the same
// reason). Each function assumes statement()'s dispatch in parser.c
// already consumed the keyword token that identified it.

void if_statement(void) {
    advance_token();
    expect(TOKEN_LPAREN, "expected '(' after 'if'");
    codegen_expression();
    expect(TOKEN_RPAREN, "expected ')' after condition");
    pop_and_test_truthy();
    int else_jump = emit_jcc_rel32(code, COND_E);

    block();
    if (current.type == TOKEN_ELSE) {
        int end_jump = emit_jmp_rel32(code);
        emit_patch_jump(code, else_jump);
        advance_token();
        block();
        emit_patch_jump(code, end_jump);
    } else {
        emit_patch_jump(code, else_jump);
    }
}

void while_statement(void) {
    advance_token();
    expect(TOKEN_LPAREN, "expected '(' after 'while'");
    int loop_start = code->count;
    codegen_expression();
    expect(TOKEN_RPAREN, "expected ')' after condition");
    pop_and_test_truthy();
    int exit_jump = emit_jcc_rel32(code, COND_E);

    loop_push();
    block();
    loop_patch_continues(); // continue lands here, right before the loop-back
    emit_jmp_back(code, loop_start);
    emit_patch_jump(code, exit_jump);
    loop_pop_and_patch_breaks(); // break lands here, after the loop
}

// fn NAME ( [IDENT (, IDENT)*] ) block
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
// `let` -- goes through the exact same var_slot_*_offset() formulas.
// Because the last argument is the last one pushed (closest to the
// return address), and the first argument is deepest, parameter i's tag
// sits at [rbp + 16 + 16*(argc-1-i)] and its payload right after that
// (+16 skips the saved rbp and return address). Return value comes back
// as RBX=tag, RAX=payload (see parser.c's return_statement).
void fn_statement(void) {
    advance_token();
    expect(TOKEN_IDENTIFIER, "expected function name after 'fn'");
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

    VarScope outer_vars;
    vars_save(&outer_vars);
    vars_clear();
    int outer_for_depth;
    for_depth_save(&outer_for_depth);
    for_depth_reset();
    int outer_loop_depth;
    loop_depth_save(&outer_loop_depth);
    loop_depth_reset();

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

    // Implicit `return 0;` if the body falls through without one --
    // dead code on any path that already returned explicitly, harmless
    // (matches ashvm's compile_fn_decl, which does the same
    // unconditionally after compiling the body).
    emit_mov_reg_imm64(code, REG_RAX, 0); // 0.0's bits are all zero
    emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
    emit_mov_reg_reg(code, REG_RSP, REG_RBP);
    emit_pop_reg(code, REG_RBP);
    emit_ret(code);

    vars_restore(&outer_vars);
    for_depth_restore(outer_for_depth);
    loop_depth_restore(outer_loop_depth);
    emit_patch_jump(code, skip_jump);
}
