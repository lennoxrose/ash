#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compiler/internal.h"
#include "diagnostics/diagnostics.h"

// statement() is shared between top-level parsing (compile()'s own loop,
// and stmt_import.c's parse_imported_file_body()) and here, inside a
// fn/if/while/try body -- block_depth distinguishes the two so the "only
// fn/let at an imported file's top level" guard in statement() only
// fires for genuinely top-level statements, never for an ordinary
// return/if/etc. nested inside an imported function's own body.
static int block_depth = 0;

void block(void) {
    expect(TOKEN_LBRACE, "expected '{'");
    block_depth++;
    while (current.type != TOKEN_RBRACE && current.type != TOKEN_EOF) statement();
    block_depth--;
    expect(TOKEN_RBRACE, "expected '}'");
}

// let name = value;  --  also tries to fuse `let x = x OP (number|local);`
// into a single OP_ACC_LOCAL instead of a full expression compile, when the
// shape matches exactly (see vm/compiler.c's history / rundown.md).
static void let_statement(void) {
    advance_token();
    expect(TOKEN_IDENTIFIER, "expected variable name after 'let'");
    const char *name = previous.start;
    int len = previous.length;
    expect(TOKEN_EQUAL, "expected '=' after variable name");

    if (try_declare_import_constant(name, len)) return;

    int slot = resolve_local(name, len);

    int fused = 0;
    if (slot != -1 && current.type == TOKEN_IDENTIFIER &&
        current.length == len && strncmp(current.start, name, len) == 0) {
        Token save = current;
        advance_token();
        int op_code = -1;
        if (current.type == TOKEN_PLUS) op_code = 0;
        else if (current.type == TOKEN_MINUS) op_code = 1;
        else if (current.type == TOKEN_STAR) op_code = 2;

        if (op_code != -1) {
            advance_token();
            uint8_t flags = (uint8_t)op_code;
            uint8_t b_operand;
            int have_operand = 0;
            if (current.type == TOKEN_NUMBER) {
                double v = strtod(current.start, NULL);
                b_operand = (uint8_t)chunk_add_constant(chunk, vm_num(v));
                flags |= 0x80;
                advance_token();
                have_operand = 1;
            } else if (current.type == TOKEN_IDENTIFIER) {
                int src_slot = resolve_local(current.start, current.length);
                if (src_slot != -1) {
                    b_operand = (uint8_t)src_slot;
                    advance_token();
                    have_operand = 1;
                } else {
                    b_operand = 0;
                }
            } else {
                b_operand = 0;
            }

            if (have_operand && current.type == TOKEN_SEMICOLON) {
                advance_token();
                emit_op(OP_ACC_LOCAL);
                emit((uint8_t)slot);
                emit(flags);
                emit(b_operand);
                fused = 1;
            }
        }
        if (!fused) { lexer_init(save.start + save.length); current = save; }
    }

    if (!fused) {
        expression();
        expect(TOKEN_SEMICOLON, "expected ';' after statement");
        if (slot != -1) { emit2(OP_SET_LOCAL, (uint8_t)slot); emit_op(OP_POP); }
        else declare_local(name, len);
    }
}

// Either a bare call (`foo();`) or an index assignment (`foo[i] = value;`) --
// the two statement shapes that start with a bare identifier.
static void identifier_statement(void) {
    Token id = current;
    advance_token();
    if (current.type == TOKEN_LPAREN) {
        advance_token();
        emit_call(id);
        expect(TOKEN_SEMICOLON, "expected ';' after call");
        emit_op(OP_POP);
        return;
    }
    if (current.type == TOKEN_LBRACKET) {
        int slot = resolve_local(id.start, id.length);
        if (slot == -1) {
            char msg[128];
            snprintf(msg, sizeof(msg), "undefined variable: %.*s", id.length, id.start);
            diagnostics_report("error", msg, id.start, id.length);
            exit(1);
        }
        emit2(OP_GET_LOCAL, (uint8_t)slot);
        advance_token();
        expression();
        expect(TOKEN_RBRACKET, "expected ']'");
        expect(TOKEN_EQUAL, "expected '=' for index assignment");
        expression();
        expect(TOKEN_SEMICOLON, "expected ';' after assignment");
        emit_op(OP_INDEX_SET);
        return;
    }
    diagnostics_report("error", "unexpected identifier statement", current.start, current.length);
    exit(1);
}

void statement(void) {
    if (current.type == TOKEN_IMPORT) { advance_token(); compile_import_stmt(); return; }
    imports_close_block();

    const char *ns; int ns_len;
    get_import_namespace(&ns, &ns_len);
    if (ns_len > 0 && block_depth == 0 && current.type != TOKEN_FN && current.type != TOKEN_LET) {
        diagnostics_report("error", "only 'fn' and constant 'let' declarations are allowed at an imported file's top level", current.start, current.length);
        exit(1);
    }

    if (current.type == TOKEN_PRINT) {
        advance_token();
        expression();
        expect(TOKEN_SEMICOLON, "expected ';' after statement");
        emit_op(OP_PRINT);
        return;
    }
    if (current.type == TOKEN_LET) { let_statement(); return; }
    if (current.type == TOKEN_RETURN) {
        advance_token();
        if (current.type != TOKEN_SEMICOLON) expression();
        else emit_constant(vm_num(0));
        expect(TOKEN_SEMICOLON, "expected ';' after return");
        emit_op(OP_RETURN);
        return;
    }
    if (current.type == TOKEN_FN) { advance_token(); compile_fn_decl(); return; }
    if (current.type == TOKEN_IF) { advance_token(); compile_if_stmt(); return; }
    if (current.type == TOKEN_TRY) { advance_token(); compile_try_stmt(); return; }
    if (current.type == TOKEN_WHILE) { advance_token(); compile_while_stmt(); return; }
    if (current.type == TOKEN_IDENTIFIER) { identifier_statement(); return; }

    diagnostics_report("error", "unexpected token", current.start, current.length);
    exit(1);
}

static Chunk main_chunk;

Chunk *compile(const char *source, const char *source_path) {
    vm_function_count = 0;
    local_count = 0;
    chunk_init(&main_chunk);
    chunk = &main_chunk;
    lexer_init(source);
    advance_token();
    imports_init(source_path);
    while (current.type != TOKEN_EOF) statement();
    return &main_chunk;
}

// Appends compiled bytecode to an EXISTING chunk without resetting locals or
// function definitions -- used by the REPL so state persists between lines.
Chunk *compile_repl_line(const char *source, Chunk *target_chunk, int *out_start_offset) {
    chunk = target_chunk;
    *out_start_offset = chunk->count;
    lexer_init(source);
    advance_token();
    while (current.type != TOKEN_EOF) statement();
    return chunk;
}
