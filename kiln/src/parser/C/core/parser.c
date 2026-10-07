#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser/H/core/parser.h"
#include "parser/H/core/parser_internal.h"
#include "parser/H/declarations/vars.h"
#include "parser/H/statements/loop_stack.h"
#include "parser/H/declarations/functions.h"
#include "parser/H/imports/imports.h"
#include "parser/H/declarations/constants.h"
#include "codegen/H/expressions/expr.h"
#include "codegen/H/emit/emit_sse.h"
#include "codegen/H/emit/value.h"
#include "codegen/H/runtime/heap.h"
#include "codegen/H/emit/bytes.h"
#include "codegen/H/strings/string_alloc.h"
#include "codegen/H/collections/arrays.h"
#include "codegen/H/collections/maps.h"
#include "codegen/H/runtime/errors.h"
#include "codegen/H/runtime/print_int.h"
#include "diagnostics/H/diagnostics.h"
#include "elf/H/elf_writer.h"
#include "app/H/target.h"

Token current;
Token previous;
CodeBuf *code;

void advance_token(void) { previous = current; current = lexer_next_token(); }

void parse_error(const char *message) {
    diagnostics_report("error", message, current.start, current.length);
    exit(1);
}

void expect(TokenType type, const char *message) {
    if (current.type == type) { advance_token(); return; }
    parse_error(message);
}

// statement() is shared between top-level parsing (compile_program's own
// loop, and imports.c's parse_imported_file_body()) and here, inside a
// fn/if/while/for/try body -- block_depth distinguishes the two so the
// "only fn/let at an imported file's top level" guard in statement()
// only fires for genuinely top-level statements, never for an ordinary
// `return`/`if`/etc. nested inside an imported function's own body.
static int block_depth = 0;

void block(void) {
    VarBlockScope saved_scope = vars_scope_begin();
    expect(TOKEN_LBRACE, "expected '{'");
    block_depth++;
    while (current.type != TOKEN_RBRACE && current.type != TOKEN_EOF) statement();
    block_depth--;
    expect(TOKEN_RBRACE, "expected '}'");
    vars_scope_end(saved_scope);
}

// let IDENT = expr ;  -- reassigns the existing slot if `name` is already
// declared instead of erroring, matching ashvm's own let-statement
// behavior exactly (ashvm/src/compiler/stmt.c's let_statement).
static void let_statement(void) {
    advance_token();
    expect(TOKEN_IDENTIFIER, "expected variable name after 'let'");
    const char *name = previous.start;
    int len = previous.length;
    expect(TOKEN_EQUAL, "expected '=' after variable name");

    const char *ns; int ns_len;
    get_import_namespace(&ns, &ns_len);
    if (ns_len > 0) {
        char combined[96];
        int combined_len = snprintf(combined, sizeof(combined), "%.*s.%.*s", ns_len, ns, len, name);
        if (current.type == TOKEN_NUMBER) {
            char clean[128];
            int clean_len = 0;
            for (int i = 0; i < current.length; i++) {
                if (current.start[i] == '_') continue;
                clean[clean_len++] = current.start[i];
            }
            clean[clean_len] = '\0';
            double v = strtod(clean, NULL);
            advance_token();
            declare_number_constant(combined, combined_len, v);
        } else if (current.type == TOKEN_STRING) {
            declare_string_constant(combined, combined_len, current.start, current.length);
            advance_token();
        } else {
            parse_error("imported top-level 'let' must be a literal number or string constant");
        }
        expect(TOKEN_SEMICOLON, "expected ';' after statement");
        return;
    }

    codegen_expression();
    expect(TOKEN_SEMICOLON, "expected ';' after statement");

    int slot = resolve_var_in_current_scope(name, len);
    if (slot == -1) slot = declare_var(name, len);
    emit_pop_reg(code, REG_RAX); // payload
    emit_pop_reg(code, REG_RBX); // tag
    emit_store_mem_disp32(code, REG_RBP, var_slot_tag_offset(slot), REG_RBX);
    emit_store_mem_disp32(code, REG_RBP, var_slot_payload_offset(slot), REG_RAX);
}

// IDENT[index] = expr ;  -- writes through an existing array OR map
// variable (which one is only known at runtime -- see expr.c's postfix()
// for the read-side version of this same dispatch).
static void index_assignment_statement(Token id) {
    int slot = resolve_var(id.start, id.length);
    if (slot == -1) {
        char msg[128];
        snprintf(msg, sizeof(msg), "undefined variable: %.*s", id.length, id.start);
        parse_error(msg);
    }
    emit_load_mem_disp32(code, REG_RBX, REG_RBP, var_slot_tag_offset(slot));
    emit_push_reg(code, REG_RBX);
    emit_load_mem_disp32(code, REG_RAX, REG_RBP, var_slot_payload_offset(slot));
    emit_push_reg(code, REG_RAX);

    advance_token(); // consume '['
    codegen_expression();
    expect(TOKEN_RBRACKET, "expected ']' after index");
    expect(TOKEN_EQUAL, "expected '=' after index");
    codegen_expression();
    expect(TOKEN_SEMICOLON, "expected ';' after assignment");

    emit_pop_reg(code, REG_RAX); // value payload
    emit_pop_reg(code, REG_RBX); // value tag
    emit_pop_reg(code, REG_RCX); // index payload
    emit_pop_reg(code, REG_RDX); // index tag
    emit_pop_reg(code, REG_RDI); // base payload
    emit_pop_reg(code, REG_RSI); // base tag
    emit_push_reg(code, REG_RSI);
    emit_push_reg(code, REG_RDI);
    emit_push_reg(code, REG_RDX);
    emit_push_reg(code, REG_RCX);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
    // Strings are immutable -- checked here rather than left to fall
    // through to the array path (see missing.md #1: that path assumes
    // its base is an array object and would corrupt memory given a
    // string's raw content bytes instead).
    emit_cmp_reg_imm32(code, REG_RSI, TAG_STRING);
    int not_string = emit_jcc_rel32(code, COND_NE);
    errors_emit_die(code, "runtime error: cannot assign to a string index");
    emit_patch_jump(code, not_string);
    emit_cmp_reg_imm32(code, REG_RSI, TAG_MAP);
    int is_map = emit_jcc_rel32(code, COND_E);
    codegen_array_index_store();
    int done = emit_jmp_rel32(code);
    emit_patch_jump(code, is_map);
    codegen_map_index_store();
    emit_patch_jump(code, done);
}

// f(args);  -- a call used for its side effect, result discarded.
static void call_statement(Token id) {
    advance_token(); // consume '('
    codegen_call(id);
    expect(TOKEN_SEMICOLON, "expected ';' after call");
    emit_pop_reg(code, REG_RAX); // discard the (unused) return value
    emit_pop_reg(code, REG_RBX);
}

static int resolve_var_or_error(Token id) {
    int slot = resolve_var(id.start, id.length);
    if (slot == -1) {
        char msg[128];
        snprintf(msg, sizeof(msg), "undefined variable: %.*s", id.length, id.start);
        parse_error(msg);
    }
    return slot;
}

// x += e ; / x -= e ; / x *= e ; / x /= e ;  -- sugar for x = x <op> e.
// `+=` reuses codegen_apply_plus() (the exact same tag-aware string-concat-
// or-numeric-add logic `+` itself uses); `-=`/`*=`/`/=` are NUMBER-only,
// same scope limit as `-`/`*`/`/` themselves.
static void compound_assignment_statement(Token id, TokenType op) {
    int slot = resolve_var_or_error(id);
    advance_token(); // consume the += / -= / *= / /=

    emit_load_mem_disp32(code, REG_RBX, REG_RBP, var_slot_tag_offset(slot));
    emit_push_reg(code, REG_RBX);
    emit_load_mem_disp32(code, REG_RAX, REG_RBP, var_slot_payload_offset(slot));
    emit_push_reg(code, REG_RAX);

    codegen_expression();
    expect(TOKEN_SEMICOLON, "expected ';' after assignment");

    emit_pop_reg(code, REG_RCX); // b payload
    emit_pop_reg(code, REG_RDX); // b tag
    emit_pop_reg(code, REG_RAX); // a payload
    emit_pop_reg(code, REG_RBX); // a tag

    if (op == TOKEN_PLUS_EQUAL) {
        codegen_apply_plus();
    } else {
        emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
        emit_movq_xmm_from_reg(code, XMM1, REG_RCX);
        if (op == TOKEN_MINUS_EQUAL) emit_subsd(code, XMM0, XMM1);
        else if (op == TOKEN_STAR_EQUAL) emit_mulsd(code, XMM0, XMM1);
        else emit_divsd(code, XMM0, XMM1); // TOKEN_SLASH_EQUAL
        emit_movq_reg_from_xmm(code, REG_RAX, XMM0);
        emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
        emit_push_reg(code, REG_RBX);
        emit_push_reg(code, REG_RAX);
    }

    emit_pop_reg(code, REG_RAX); // result payload
    emit_pop_reg(code, REG_RBX); // result tag
    emit_store_mem_disp32(code, REG_RBP, var_slot_tag_offset(slot), REG_RBX);
    emit_store_mem_disp32(code, REG_RBP, var_slot_payload_offset(slot), REG_RAX);
}

// x++ ; / x-- ;  -- sugar for x = x + 1 / x = x - 1. Statement-only (not
// a pre/post-increment EXPRESSION with a value of its own -- that needs
// primary()/postfix() changes this project's scope doesn't call for).
static void incdec_statement(Token id, TokenType op) {
    int slot = resolve_var_or_error(id);
    advance_token(); // consume ++ or --
    expect(TOKEN_SEMICOLON, "expected ';' after statement");

    emit_load_mem_disp32(code, REG_RAX, REG_RBP, var_slot_payload_offset(slot));
    emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
    double one = 1.0;
    uint64_t one_bits;
    memcpy(&one_bits, &one, sizeof(one_bits));
    emit_mov_reg_imm64(code, REG_RBX, one_bits);
    emit_movq_xmm_from_reg(code, XMM1, REG_RBX);
    if (op == TOKEN_PLUS_PLUS) emit_addsd(code, XMM0, XMM1);
    else emit_subsd(code, XMM0, XMM1);
    emit_movq_reg_from_xmm(code, REG_RAX, XMM0);
    emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
    emit_store_mem_disp32(code, REG_RBP, var_slot_tag_offset(slot), REG_RBX);
    emit_store_mem_disp32(code, REG_RBP, var_slot_payload_offset(slot), REG_RAX);
}

// IDENT = expr ;  -- `name` must already be declared via `let`.
static void assignment_statement(void) {
    Token id = current;
    advance_token();
    if (current.type == TOKEN_LBRACKET) { index_assignment_statement(id); return; }
    if (current.type == TOKEN_LPAREN) { call_statement(id); return; }
    if (current.type == TOKEN_PLUS_EQUAL || current.type == TOKEN_MINUS_EQUAL ||
        current.type == TOKEN_STAR_EQUAL || current.type == TOKEN_SLASH_EQUAL) {
        compound_assignment_statement(id, current.type);
        return;
    }
    if (current.type == TOKEN_PLUS_PLUS || current.type == TOKEN_MINUS_MINUS) {
        incdec_statement(id, current.type);
        return;
    }
    expect(TOKEN_EQUAL, "expected '=' after variable name");
    codegen_expression();
    expect(TOKEN_SEMICOLON, "expected ';' after assignment");

    int slot = resolve_var_or_error(id);
    emit_pop_reg(code, REG_RAX); // payload
    emit_pop_reg(code, REG_RBX); // tag
    emit_store_mem_disp32(code, REG_RBP, var_slot_tag_offset(slot), REG_RBX);
    emit_store_mem_disp32(code, REG_RBP, var_slot_payload_offset(slot), REG_RAX);
}

// return [expr] ;  -- bare `return;` returns 0, matching ashc/ashvm.
// Return value comes back as RBX=tag, RAX=payload (see codegen/C/expressions/expr.c's
// call-site handling).
static void return_statement(void) {
    advance_token();
    if (current.type != TOKEN_SEMICOLON) {
        codegen_expression();
        expect(TOKEN_SEMICOLON, "expected ';' after return");
        emit_pop_reg(code, REG_RAX); // payload
        emit_pop_reg(code, REG_RBX); // tag
    } else {
        advance_token();
        emit_mov_reg_imm64(code, REG_RAX, 0); // 0.0's bits are all zero
        emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
    }
    emit_mov_reg_reg(code, REG_RSP, REG_RBP);
    emit_pop_reg(code, REG_RBP);
    emit_ret(code);
}

// throw expr ;  -- expr must be a STRING (matches try/catch's own
// convention: every caught error, built-in or user-thrown, is a plain
// string). Reuses codegen/C/runtime/errors.c's raise routine directly -- the exact
// same catch/unwind mechanism M9's built-in errors (array bounds, missing
// map key) already go through, just with a runtime message instead of a
// compile-time-constant one.
static void throw_statement(void) {
    advance_token();
    codegen_expression();
    expect(TOKEN_SEMICOLON, "expected ';' after 'throw'");

    emit_pop_reg(code, REG_RSI); // message payload (assumed STRING)
    emit_pop_reg(code, REG_RBX); // message tag (ignored)
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, -8); // length
    errors_emit_die_dynamic(code);
}

// break ;  -- jumps past the innermost loop; the jump is recorded, not
// resolved here, since the innermost loop's own codegen (parser_control.c's
// while_statement / parser/C/statements/for_loop.c's for_statement) hasn't emitted the
// "after the loop" landing point yet.
static void break_statement(void) {
    advance_token();
    expect(TOKEN_SEMICOLON, "expected ';' after 'break'");
    loop_record_break(emit_jmp_rel32(code));
}

// continue ;  -- jumps to the innermost loop's per-iteration advance
// step (a for-loop's index increment, or straight back to the condition
// for a while loop) -- also recorded, not resolved here, for the same
// reason as break above.
static void continue_statement(void) {
    advance_token();
    expect(TOKEN_SEMICOLON, "expected ';' after 'continue'");
    loop_record_continue(emit_jmp_rel32(code));
}

// statement := "print" expr ";"
//            | "let" IDENT "=" expr ";"
//            | IDENT "=" expr ";"
//            | "if" "(" expr ")" block ["else" block]
//            | "while" "(" expr ")" block
//            | "fn" IDENT "(" params ")" block
//            | "return" [expr] ";"
// (milestone 5's entire statement grammar -- anything else is a parse
// error, per the project rule that malformed input always fails cleanly)
void statement(void) {
    if (current.type == TOKEN_IMPORT) { import_statement(); return; }
    imports_close_block();

    const char *ns; int ns_len;
    get_import_namespace(&ns, &ns_len);
    if (ns_len > 0 && block_depth == 0 && current.type != TOKEN_FN && current.type != TOKEN_LET) {
        parse_error("only 'fn' and constant 'let' declarations are allowed at an imported file's top level");
    }

    if (current.type == TOKEN_PRINT) {
        advance_token();
        codegen_expression();
        expect(TOKEN_SEMICOLON, "expected ';' after print statement");
        codegen_print_top(code);
        return;
    }
    if (current.type == TOKEN_LET) { let_statement(); return; }
    if (current.type == TOKEN_IF) { if_statement(); return; }
    if (current.type == TOKEN_WHILE) { while_statement(); return; }
    if (current.type == TOKEN_FN) { fn_statement(); return; }
    if (current.type == TOKEN_RETURN) { return_statement(); return; }
    if (current.type == TOKEN_TRY) { try_statement(); return; }
    if (current.type == TOKEN_FOR) { for_statement(); return; }
    if (current.type == TOKEN_BREAK) { break_statement(); return; }
    if (current.type == TOKEN_CONTINUE) { continue_statement(); return; }
    if (current.type == TOKEN_THROW) { throw_statement(); return; }
    if (current.type == TOKEN_IDENTIFIER) { assignment_statement(); return; }
    parse_error("expected 'print', 'let', 'if', 'while', 'fn', 'return', 'try', 'for', 'break', 'continue', 'throw', '@import', or an assignment");
}

void compile_program(const char *source, const char *source_path, CodeBuf *out) {
    code = out;
    code_init(code);
    lexer_init(source);
    advance_token();
    imports_init(source_path);

    // The kernel hands a freshly-started process RSP pointing at
    // [argc, argv[0], ..., NULL, envp...] -- this is the ONLY point in
    // the whole program where that original RSP is still available
    // (nothing has touched it yet), so it's saved to a fixed globals
    // address here, before anything else, for argv() to read later from
    // any frame (see codegen/C/io/argv_builtin.c and elf/H/elf_writer.h's
    // KILN_ARGV_ADDR comment). Windows has no equivalent need: argv()
    // there calls GetCommandLineA() directly, callable at any point, so
    // there's nothing to snapshot this early.
    if (kiln_get_target() != KILN_TARGET_WINDOWS) {
        emit_mov_reg_imm64(code, REG_RSI, KILN_ARGV_ADDR);
        emit_store_mem_disp32(code, REG_RSI, 0, REG_RSP);
    }

    // Frame prologue: RBP anchors the top-level program's own variable
    // slots. RSP drifts during expression push/pop; RBP never does. Each
    // function call below establishes and tears down its own separate
    // frame (see parser_control.c's fn_statement/parser.c's
    // return_statement) without disturbing this one -- the callee always
    // restores RBP before returning. Milestone 5: each slot is now 16
    // bytes (tag + payload, see codegen/H/emit/value.h), not 8.
    emit_mov_reg_reg(code, REG_RBP, REG_RSP);
    emit_sub_reg_imm32(code, REG_RSP, KILN_FRAME_RESERVE);

    heap_emit_startup(code);
    bytes_emit_startup(code);
    string_alloc_emit_startup(code); // after heap_emit_startup: calls heap_emit_alloc internally
    print_emit_startup(code);
    errors_emit_startup(code);

    while (current.type != TOKEN_EOF) statement();
    codegen_exit0(code);
}
