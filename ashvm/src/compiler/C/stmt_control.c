#include <stdlib.h>
#include <string.h>
#include "compiler/H/internal.h"
#include "diagnostics/H/diagnostics.h"

// Each function here assumes statement() already consumed the keyword token
// (TOKEN_FN / TOKEN_IF / TOKEN_TRY / TOKEN_WHILE) that identified it.

void compile_fn_decl(void) {
    expect(TOKEN_IDENTIFIER, "expected function name after 'fn'");
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

    chunk = &vm_functions[fn_idx].chunk;
    local_count = 0;
    for (int i = 0; i < argc; i++) declare_local(param_names[i], param_lens[i]);

    block();
    emit_constant(vm_num(0));
    emit_op(OP_RETURN);

    chunk = outer_chunk;
    local_count = outer_local_count;
    memcpy(local_names, outer_locals, sizeof(local_names));
}

void compile_if_stmt(void) {
    expect(TOKEN_LPAREN, "expected '(' after 'if'");
    int patch_loc = try_fuse_condition();
    int else_jump;
    if (patch_loc != -1) {
        expect(TOKEN_RPAREN, "expected ')' after condition");
        else_jump = patch_loc;
    } else {
        expression();
        expect(TOKEN_RPAREN, "expected ')' after condition");
        else_jump = emit_jump(OP_JUMP_IF_FALSE);
    }
    block();
    if (current.type == TOKEN_ELSE) {
        int end_jump = emit_jump(OP_JUMP);
        patch_jump(else_jump);
        advance_token();
        block();
        patch_jump(end_jump);
    } else {
        patch_jump(else_jump);
    }
}

void compile_try_stmt(void) {
    int saved_local_count = local_count;

    int try_jump = emit_jump(OP_TRY_PUSH);

    block();

    emit_op(OP_TRY_POP);
    int skip_catch = emit_jump(OP_JUMP);

    patch_jump(try_jump);

    expect(TOKEN_CATCH, "expected 'catch' after try block");
    expect(TOKEN_LPAREN, "expected '(' after 'catch'");
    expect(TOKEN_IDENTIFIER, "expected error variable name");
    // 'e' must land at EXACTLY saved_local_count, not wherever local_count
    // ended up after compiling the try block -- since the try block may have
    // declared its own locals before erroring, but the runtime error-value
    // push always lands at the try-entry stack position, regardless of what
    // the try block attempted (all of that gets discarded on the error path).
    memcpy(local_names[saved_local_count], previous.start, previous.length);
    local_names[saved_local_count][previous.length] = '\0';
    local_count = saved_local_count + 1;
    expect(TOKEN_RPAREN, "expected ')' after catch variable");

    block();
    emit_op(OP_POP);

    patch_jump(skip_catch);

    local_count = saved_local_count;
}

void compile_while_stmt(void) {
    expect(TOKEN_LPAREN, "expected '(' after 'while'");
    int loop_start = chunk->count;
    int patch_loc = try_fuse_condition();
    int exit_jump;
    if (patch_loc != -1) {
        expect(TOKEN_RPAREN, "expected ')' after condition");
        exit_jump = patch_loc;
    } else {
        expression();
        expect(TOKEN_RPAREN, "expected ')' after condition");
        exit_jump = emit_jump(OP_JUMP_IF_FALSE);
    }
    block();
    emit_loop(loop_start);
    patch_jump(exit_jump);
}
