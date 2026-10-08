#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser/H/core/parser.h"
#include "parser/H/core/parser_internal.h"
#include "parser/H/declarations/vars.h"
#include "parser/H/declarations/module_state.h"
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
// forge/given/during/each/attempt body -- block_depth distinguishes the
// two so the "only forge/local at an imported file's top level" guard in
// statement() only fires for genuinely top-level statements, never for an
// ordinary `yield`/`given`/etc. nested inside an imported function's own
// body.
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

// local IDENT = expr ;  -- reassigns the existing slot if `name` is already
// declared instead of erroring, matching ashvm's own local-statement
// behavior exactly (ashvm/src/compiler/stmt.c's local_statement).
static void local_statement(void) {
    advance_token();
    expect(TOKEN_IDENTIFIER, "expected variable name after 'local'");
    const char *name = previous.start;
    int len = previous.length;
    expect(TOKEN_EQUAL, "expected '=' after variable name");

    const char *ns; int ns_len;
    get_import_namespace(&ns, &ns_len);
    // Bug (found while writing the first real forgepack module, a JSON
    // codec -- any imported function with a `local` inside a loop failed
    // to compile, mirrored from the identical ashvm bug): this only
    // makes sense at block_depth == 0 (a genuinely top-level `local` in
    // the imported file), not for an ordinary local deep inside one of
    // that file's own functions -- the sibling guard further down in
    // statement() already gets this right, this one didn't.
    if (ns_len > 0 && block_depth == 0) {
        // A top-level `local` of an imported file is a module-level variable: the
        // expression runs once, at the point of the `@import`, and the value lives
        // in a global slot every function of that file (and `ns.NAME`) can read,
        // and `NAME = ...;` can update (see parser/H/declarations/module_state.h).
        int state_slot = module_state_declare(name, len);
        codegen_expression();
        expect(TOKEN_SEMICOLON, "expected ';' after statement");
        module_state_emit_store(state_slot);
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

// A variable that statements can read and write: either a frame slot of the
// current function, or (inside an imported file) a module-level variable.
typedef struct { int global; int index; } VarRef;

static VarRef resolve_ref_or_error(Token id) {
    int slot = resolve_var(id.start, id.length);
    if (slot != -1) return (VarRef){ 0, slot };
    int state = module_state_resolve(id.start, id.length);
    if (state != -1) return (VarRef){ 1, state };
    char msg[128];
    snprintf(msg, sizeof(msg), "undefined variable: %.*s", id.length, id.start);
    parse_error(msg);
    return (VarRef){ 0, -1 };
}

static void ref_push(VarRef r) {
    if (r.global) { module_state_emit_load(r.index); return; }
    emit_load_mem_disp32(code, REG_RBX, REG_RBP, var_slot_tag_offset(r.index));
    emit_push_reg(code, REG_RBX);
    emit_load_mem_disp32(code, REG_RAX, REG_RBP, var_slot_payload_offset(r.index));
    emit_push_reg(code, REG_RAX);
}

// Stores RBX (tag) / RAX (payload) into the variable.
static void ref_store_regs(VarRef r) {
    if (r.global) {
        emit_push_reg(code, REG_RBX);
        emit_push_reg(code, REG_RAX);
        module_state_emit_store(r.index);
        return;
    }
    emit_store_mem_disp32(code, REG_RBP, var_slot_tag_offset(r.index), REG_RBX);
    emit_store_mem_disp32(code, REG_RBP, var_slot_payload_offset(r.index), REG_RAX);
}

// IDENT[index] = expr ; / IDENT[i1][i2]...[iN] = expr ;  -- writes
// through an existing array OR map variable (which one is only known at
// runtime -- see expr.c's postfix() for the read-side version of this
// same dispatch). Every index but the last is a READ (codegen_index_read,
// the same primitive postfix() chains on the read side): C[i][j] = val
// first reads C[i] to get the inner array, then stores val into THAT
// array at [j] -- it never needs to write the outer container back,
// since the inner array it just read out is the very same heap object
// C[i] already points at (arrays/maps are reference values here, not
// copied on read).
static void index_assignment_statement(Token id) {
    ref_push(resolve_ref_or_error(id));

    advance_token(); // consume '['
    codegen_expression();
    expect(TOKEN_RBRACKET, "expected ']' after index");
    while (current.type == TOKEN_LBRACKET) {
        codegen_index_read(); // stack: [base] -> [base[index]]
        advance_token(); // consume '['
        codegen_expression();
        expect(TOKEN_RBRACKET, "expected ']' after index");
    }
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

// x += e ; / x -= e ; / x *= e ; / x /= e ;  -- sugar for x = x <op> e.
// `+=` reuses codegen_apply_plus() (the exact same tag-aware string-concat-
// or-numeric-add logic `+` itself uses); `-=`/`*=`/`/=` are NUMBER-only,
// same scope limit as `-`/`*`/`/` themselves.
static void compound_assignment_statement(Token id, TokenType op) {
    VarRef ref = resolve_ref_or_error(id);
    advance_token(); // consume the += / -= / *= / /=

    ref_push(ref);

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
    ref_store_regs(ref);
}

// x++ ; / x-- ;  -- sugar for x = x + 1 / x = x - 1. Statement-only (not
// a pre/post-increment EXPRESSION with a value of its own -- that needs
// primary()/postfix() changes this project's scope doesn't call for).
static void incdec_statement(Token id, TokenType op) {
    VarRef ref = resolve_ref_or_error(id);
    advance_token(); // consume ++ or --
    expect(TOKEN_SEMICOLON, "expected ';' after statement");

    ref_push(ref);
    emit_pop_reg(code, REG_RAX); // payload
    emit_pop_reg(code, REG_RBX); // tag (ignored)
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
    ref_store_regs(ref);
}

// IDENT = expr ;  -- `name` must already be declared via `let`.
static void assignment_statement(void) {
    Token id = current;
    advance_token();
    // A namespaced call used as its OWN statement (`json.save(...);`,
    // discarding the result) rather than nested in an expression
    // (`local x = json.load(...);`, where codegen/C/expressions/expr.c's
    // primary() already handles this same `ident.member(...)` shape) --
    // mirrors the identical ashvm fix, found missing the same way (the
    // first real forgepack module's whole point is exactly this shape).
    if (current.type == TOKEN_DOT) {
        advance_token();
        expect(TOKEN_IDENTIFIER, "expected name after '.'");
        Token member = previous;
        char combined[128];
        int combined_len = snprintf(combined, sizeof(combined), "%.*s.%.*s",
                                      id.length, id.start, member.length, member.start);
        expect(TOKEN_LPAREN, "expected '(' after namespaced function name");
        codegen_call_named(combined, combined_len);
        expect(TOKEN_SEMICOLON, "expected ';' after call");
        emit_pop_reg(code, REG_RAX); // discard the (unused) return value
        emit_pop_reg(code, REG_RBX);
        return;
    }
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

    VarRef ref = resolve_ref_or_error(id);
    emit_pop_reg(code, REG_RAX); // payload
    emit_pop_reg(code, REG_RBX); // tag
    ref_store_regs(ref);
}

// yield [expr] ;  -- bare `yield;` returns 0, matching ashc/ashvm.
// Return value comes back as RBX=tag, RAX=payload (see codegen/C/expressions/expr.c's
// call-site handling).
static void yield_statement(void) {
    advance_token();
    if (current.type != TOKEN_SEMICOLON) {
        codegen_expression();
        expect(TOKEN_SEMICOLON, "expected ';' after yield");
        emit_pop_reg(code, REG_RAX); // payload
        emit_pop_reg(code, REG_RBX); // tag
    } else {
        advance_token();
        emit_mov_reg_imm64(code, REG_RAX, 0); // 0.0's bits are all zero
        emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
    }
    emit_attempt_unwind_all(); // leave open attempts; only touches RCX/RDX, so the result in RAX/RBX survives
    emit_mov_reg_reg(code, REG_RSP, REG_RBP);
    emit_pop_reg(code, REG_RBP);
    emit_ret(code);
}

// raise expr ;  -- expr may be any value: a string (what every built-in
// runtime error raises) or, say, a map for structured errors. Reuses codegen/C/runtime/errors.c's raise routine directly --
// the exact same handle/unwind mechanism M9's built-in errors (array
// bounds, missing map key) already go through, just with a runtime
// message instead of a compile-time-constant one. (The runtime "raise
// routine" this calls into was already named that before the `throw`
// keyword became `raise` -- a coincidental match, not a rename.)
static void raise_statement(void) {
    advance_token();
    codegen_expression();
    expect(TOKEN_SEMICOLON, "expected ';' after 'raise'");

    emit_pop_reg(code, REG_RSI); // raised payload
    emit_pop_reg(code, REG_RBX); // raised tag
    emit_cmp_reg_imm32(code, REG_RBX, TAG_STRING);
    int not_text = emit_jcc_rel32(code, COND_NE);
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, -8); // length (strings only)
    emit_patch_jump(code, not_text);
    errors_emit_die_dynamic(code);
}

// stop ;  -- jumps past the innermost loop; the jump is recorded, not
// resolved here, since the innermost loop's own codegen (parser_control.c's
// during_statement / parser/C/statements/each_loop.c's each_statement) hasn't emitted the
// "after the loop" landing point yet.
static void stop_statement(void) {
    advance_token();
    expect(TOKEN_SEMICOLON, "expected ';' after 'stop'");
    emit_attempt_unwind_to_loop();
    loop_record_stop(emit_jmp_rel32(code));
}

// next ;  -- jumps to the innermost loop's per-iteration advance
// step (an each-loop's index increment, or straight back to the condition
// for a during loop) -- also recorded, not resolved here, for the same
// reason as stop above.
static void next_statement(void) {
    advance_token();
    expect(TOKEN_SEMICOLON, "expected ';' after 'next'");
    emit_attempt_unwind_to_loop();
    loop_record_next(emit_jmp_rel32(code));
}

// statement := "say" expr ";"
//            | "local" IDENT "=" expr ";"
//            | IDENT "=" expr ";"
//            | "given" ["("] expr [")"] block ["otherwise" block]
//            | "during" ["("] expr [")"] block
//            | "forge" IDENT "(" params ")" block
//            | "yield" [expr] ";"
// (milestone 5's entire statement grammar, since renamed and given optional
// condition parens -- anything else is a parse error, per the project rule
// that malformed input always fails cleanly)
void statement(void) {
    if (current.type == TOKEN_IMPORT) { import_statement(); return; }
    imports_close_block();

    const char *ns; int ns_len;
    get_import_namespace(&ns, &ns_len);
    if (ns_len > 0 && block_depth == 0 && current.type != TOKEN_FORGE && current.type != TOKEN_LOCAL) {
        parse_error("only 'forge' and constant 'local' declarations are allowed at an imported file's top level");
    }

    if (current.type == TOKEN_SAY) {
        advance_token();
        codegen_expression();
        expect(TOKEN_SEMICOLON, "expected ';' after say statement");
        codegen_print_top(code);
        return;
    }
    if (current.type == TOKEN_LOCAL) { local_statement(); return; }
    if (current.type == TOKEN_GIVEN) { given_statement(); return; }
    if (current.type == TOKEN_DURING) { during_statement(); return; }
    if (current.type == TOKEN_FORGE) { forge_statement(); return; }
    if (current.type == TOKEN_YIELD) { yield_statement(); return; }
    if (current.type == TOKEN_ATTEMPT) { attempt_statement(); return; }
    if (current.type == TOKEN_EACH) { each_statement(); return; }
    if (current.type == TOKEN_STOP) { stop_statement(); return; }
    if (current.type == TOKEN_NEXT) { next_statement(); return; }
    if (current.type == TOKEN_RAISE) { raise_statement(); return; }
    if (current.type == TOKEN_IDENTIFIER) { assignment_statement(); return; }
    parse_error("expected 'say', 'local', 'given', 'during', 'forge', 'yield', 'attempt', 'each', 'stop', 'next', 'raise', '@import', or an assignment");
}

void compile_program(const char *source, const char *source_path, CodeBuf *out) {
    code = out;
    code_init(code);
    prescan_functions(source);
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
    // frame (see parser_control.c's forge_statement/parser.c's
    // yield_statement) without disturbing this one -- the callee always
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
    check_all_functions_defined();
    codegen_exit0(code);
}
