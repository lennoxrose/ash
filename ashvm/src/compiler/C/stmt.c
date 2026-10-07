#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compiler/H/internal.h"
#include "diagnostics/H/diagnostics.h"
#include "compiler/H/loop_stack.h"

// statement() is shared between top-level parsing (compile()'s own loop,
// and stmt_import.c's parse_imported_file_body()) and here, inside a
// forge/given/during/attempt body -- block_depth distinguishes the two so
// the "only forge/local at an imported file's top level" guard in
// statement() only fires for genuinely top-level statements, never for an
// ordinary yield/given/etc. nested inside an imported function's own body.
static int block_depth = 0;

void block(void) {
    expect(TOKEN_LBRACE, "expected '{'");
    block_depth++;
    while (current.type != TOKEN_RBRACE && current.type != TOKEN_EOF) statement();
    block_depth--;
    expect(TOKEN_RBRACE, "expected '}'");
}

// local name = value;  --  also tries to fuse `local x = x OP (number|local);`
// into a single OP_ACC_LOCAL instead of a full expression compile, when the
// shape matches exactly (see vm/compiler.c's history / rundown.md).
static void local_statement(void) {
    advance_token();
    expect(TOKEN_IDENTIFIER, "expected variable name after 'local'");
    const char *name = previous.start;
    int len = previous.length;
    expect(TOKEN_EQUAL, "expected '=' after variable name");

    // Bug (found while writing the first real forgepack module, a JSON
    // codec -- any imported function with a `local` inside a loop failed
    // to compile): try_declare_import_constant() has no way to tell "a
    // top-level local in an imported file" from "an ordinary local deep
    // inside one of that file's own functions" except this check -- it
    // only makes sense at block_depth == 0, same as the sibling guard in
    // statement() above (which already gets this right).
    if (block_depth == 0 && try_declare_import_constant(name, len)) return;

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

static int resolve_local_or_error(Token id) {
    int slot = resolve_local(id.start, id.length);
    if (slot == -1) {
        char msg[128];
        snprintf(msg, sizeof(msg), "undefined variable: %.*s", id.length, id.start);
        diagnostics_report("error", msg, id.start, id.length);
        exit(1);
    }
    return slot;
}

// x += e; / x -= e; / x *= e; / x /= e;  --  sugar for x = x <op> e.
// `+=` reuses OP_ADD directly (the exact same tag-aware string-concat-or-
// numeric-add logic `+` itself uses); `-=`/`*=`/`/=` are NUMBER-only, same
// scope limit as `-`/`*`/`/` themselves. Always the general (GET_LOCAL,
// expression(), op, SET_LOCAL) path, not the OP_ACC_LOCAL fusion -- the
// RHS here is a full expression, not just a number-or-local operand.
static void compound_assignment_statement(int slot, TokenType op) {
    advance_token(); // consume the += / -= / *= / /=
    emit2(OP_GET_LOCAL, (uint8_t)slot);
    expression();
    expect(TOKEN_SEMICOLON, "expected ';' after assignment");
    switch (op) {
        case TOKEN_PLUS_EQUAL: emit_op(OP_ADD); break;
        case TOKEN_MINUS_EQUAL: emit_op(OP_SUB); break;
        case TOKEN_STAR_EQUAL: emit_op(OP_MUL); break;
        default: emit_op(OP_DIV); break; // TOKEN_SLASH_EQUAL
    }
    emit2(OP_SET_LOCAL, (uint8_t)slot);
    emit_op(OP_POP);
}

// x++; / x--;  --  sugar for x = x + 1 / x = x - 1. Statement-only (not a
// pre/post-increment EXPRESSION with a value of its own), matching kiln's
// own incdec_statement exactly.
static void incdec_statement(int slot, TokenType op) {
    advance_token(); // consume ++ or --
    expect(TOKEN_SEMICOLON, "expected ';' after statement");
    emit2(OP_GET_LOCAL, (uint8_t)slot);
    emit_constant(vm_num(1));
    emit_op(op == TOKEN_PLUS_PLUS ? OP_ADD : OP_SUB);
    emit2(OP_SET_LOCAL, (uint8_t)slot);
    emit_op(OP_POP);
}

// A bare call (`foo();`), an index assignment (`foo[i] = value;`), or a
// compound-assignment/increment-decrement on an already-declared local --
// the statement shapes that start with a bare identifier.
static void identifier_statement(void) {
    Token id = current;
    advance_token();
    // A namespaced call used as its OWN statement (`json.save(...);`,
    // discarding the result) rather than nested in an expression
    // (`local x = json.load(...);`, where expr.c's primary() already
    // handles this same `ident.member(...)` shape) -- found missing
    // while writing the first real forgepack module, whose whole point
    // (`json.save(path, value);`) is exactly this shape.
    if (current.type == TOKEN_DOT) {
        advance_token();
        expect(TOKEN_IDENTIFIER, "expected name after '.'");
        Token member = previous;
        char combined[128];
        int combined_len = snprintf(combined, sizeof(combined), "%.*s.%.*s",
                                      id.length, id.start, member.length, member.start);
        expect(TOKEN_LPAREN, "expected '(' after namespaced function name");
        emit_call_named(combined, combined_len);
        expect(TOKEN_SEMICOLON, "expected ';' after call");
        emit_op(OP_POP);
        return;
    }
    if (current.type == TOKEN_LPAREN) {
        advance_token();
        emit_call(id);
        expect(TOKEN_SEMICOLON, "expected ';' after call");
        emit_op(OP_POP);
        return;
    }
    if (current.type == TOKEN_LBRACKET) {
        int slot = resolve_local_or_error(id);
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
    if (current.type == TOKEN_PLUS_EQUAL || current.type == TOKEN_MINUS_EQUAL ||
        current.type == TOKEN_STAR_EQUAL || current.type == TOKEN_SLASH_EQUAL) {
        compound_assignment_statement(resolve_local_or_error(id), current.type);
        return;
    }
    if (current.type == TOKEN_PLUS_PLUS || current.type == TOKEN_MINUS_MINUS) {
        incdec_statement(resolve_local_or_error(id), current.type);
        return;
    }
    // Bare reassignment (`x = expr;`, no `local`) on an already-declared
    // variable -- found missing while writing the first real forgepack
    // module (a JSON codec, which reassigns a loop position variable
    // throughout). kiln already supports this (its local_statement()
    // reassigns an existing slot in place rather than requiring `local`
    // again); ashvm didn't, which is exactly the kind of "both engines
    // run the exact same code" gap this project's cross-engine audits
    // exist to catch. Plain SET_LOCAL, same as local_statement()'s own
    // `slot != -1` path -- the RHS here is a full expression, so this
    // doesn't try the OP_ACC_LOCAL fusion local_statement() attempts
    // first (that fusion only fires for a number-or-local RHS operand).
    if (current.type == TOKEN_EQUAL) {
        int slot = resolve_local_or_error(id);
        advance_token();
        expression();
        expect(TOKEN_SEMICOLON, "expected ';' after assignment");
        emit2(OP_SET_LOCAL, (uint8_t)slot);
        emit_op(OP_POP);
        return;
    }
    diagnostics_report("error", "unexpected identifier statement", current.start, current.length);
    exit(1);
}

// stop;  --  jumps past the innermost loop; the jump is recorded, not
// resolved here, since the innermost loop's own codegen
// (compile_during_stmt/compile_each_stmt in stmt_control.c) hasn't emitted
// the "after the loop" landing point yet.
static void stop_statement(void) {
    Token keyword = current;
    advance_token();
    expect(TOKEN_SEMICOLON, "expected ';' after 'stop'");
    loop_record_stop(keyword, emit_jump(OP_JUMP));
}

// next;  --  jumps to the innermost loop's per-iteration advance step (an
// each-loop's index increment, or straight back to the condition for a
// during loop) -- also recorded, not resolved here, for the same reason
// as stop above.
static void next_statement(void) {
    Token keyword = current;
    advance_token();
    expect(TOKEN_SEMICOLON, "expected ';' after 'next'");
    loop_record_next(keyword, emit_jump(OP_JUMP));
}

// raise expr;  --  expr must evaluate to a string at runtime (checked by
// the VM's do_RAISE, not here, since the raised value is a general
// expression that could be anything). Hands off to vm_raise_value, which
// performs the exact same attempt/handle unwind every other runtime error
// already goes through (vm/C/errors.c).
static void raise_statement(void) {
    advance_token();
    expression();
    expect(TOKEN_SEMICOLON, "expected ';' after 'raise'");
    emit_op(OP_RAISE);
}

void statement(void) {
    if (current.type == TOKEN_IMPORT) { advance_token(); compile_import_stmt(); return; }
    imports_close_block();

    const char *ns; int ns_len;
    get_import_namespace(&ns, &ns_len);
    if (ns_len > 0 && block_depth == 0 && current.type != TOKEN_FORGE && current.type != TOKEN_LOCAL) {
        diagnostics_report("error", "only 'forge' and constant 'local' declarations are allowed at an imported file's top level", current.start, current.length);
        exit(1);
    }

    if (current.type == TOKEN_SAY) {
        advance_token();
        expression();
        expect(TOKEN_SEMICOLON, "expected ';' after statement");
        emit_op(OP_PRINT);
        return;
    }
    if (current.type == TOKEN_LOCAL) { local_statement(); return; }
    if (current.type == TOKEN_YIELD) {
        advance_token();
        if (current.type != TOKEN_SEMICOLON) expression();
        else emit_constant(vm_num(0));
        expect(TOKEN_SEMICOLON, "expected ';' after yield");
        emit_op(OP_RETURN);
        return;
    }
    if (current.type == TOKEN_FORGE) { advance_token(); compile_forge_decl(); return; }
    if (current.type == TOKEN_GIVEN) { advance_token(); compile_given_stmt(); return; }
    if (current.type == TOKEN_ATTEMPT) { advance_token(); compile_attempt_stmt(); return; }
    if (current.type == TOKEN_DURING) { advance_token(); compile_during_stmt(); return; }
    if (current.type == TOKEN_EACH) { advance_token(); compile_each_stmt(); return; }
    if (current.type == TOKEN_STOP) { stop_statement(); return; }
    if (current.type == TOKEN_NEXT) { next_statement(); return; }
    if (current.type == TOKEN_RAISE) { raise_statement(); return; }
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
