#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen/H/expressions/expr.h"
#include "codegen/H/expressions/expr_internal.h"
#include "codegen/H/emit/emit.h"
#include "codegen/H/emit/emit_sse.h"
#include "codegen/H/emit/value.h"
#include "codegen/H/strings/strings.h"
#include "codegen/H/collections/arrays.h"
#include "codegen/H/collections/maps.h"
#include "codegen/H/builtins/builtins.h"
#include "codegen/H/functions/closures.h"
#include "parser/H/core/parser.h"
#include "parser/H/declarations/vars.h"
#include "parser/H/declarations/functions.h"
#include "parser/H/declarations/constants.h"

// Precedence climbing: expression -> logical_or -> logical_and ->
// comparison -> additive -> term -> unary -> postfix -> primary
// (comparison and up live in expr_bool.c). Every level, on completion,
// has pushed exactly
// one (tag, payload) pair onto the real CPU stack -- tag first (deeper),
// payload second (on top), so popping retrieves payload then tag.
//
// Milestone 5: a value can now be a number OR a string (codegen/H/emit/value.h),
// so both halves travel generically via GP registers (raw bit-copying
// doesn't care what the bits mean). XMM registers only ever hold the
// payload transiently, at the point of actually doing floating-point
// arithmetic on a NUMBER-tagged value -- extracted via
// emit_movq_xmm_from_reg, put back via emit_movq_reg_from_xmm.
//
// Scope limit (see the plan): only `+` and `==`/`!=` are tag-aware.
// `- * / < <= > >=` assume NUMBER operands, same as ashvm's own
// operators do when given the wrong type (ashvm raises a runtime type
// error there; kiln doesn't have runtime errors yet -- a future
// milestone, not silently pretended-away here).

static void power(void);

static void primary(void) {
    if (current.type == TOKEN_NUMBER) {
        // strtod() (real libc, called here at compile time on the host --
        // not codegen) natively parses hex ("0x1A") and scientific
        // notation ("1e10") once the lexer hands back the right token
        // boundaries (see lexer.c's number()); the one thing it doesn't
        // understand is underscore digit separators (1_000_000), so
        // those get stripped into a clean buffer first.
        char clean[128];
        int clean_len = 0;
        for (int i = 0; i < current.length; i++) {
            if (current.start[i] == '_') continue;
            if (clean_len >= (int)sizeof(clean) - 1) parse_error("number literal too long");
            clean[clean_len++] = current.start[i];
        }
        clean[clean_len] = '\0';
        double v = strtod(clean, NULL);
        advance_token();
        uint64_t bits;
        memcpy(&bits, &v, sizeof(bits));
        emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
        emit_push_reg(code, REG_RBX);
        emit_mov_reg_imm64(code, REG_RAX, bits);
        emit_push_reg(code, REG_RAX);
        return;
    }
    if (current.type == TOKEN_STRING) {
        codegen_string_literal();
        return;
    }
    if (current.type == TOKEN_FN) {
        codegen_lambda_expr();
        return;
    }
    if (current.type == TOKEN_TRUE || current.type == TOKEN_FALSE) {
        double v = current.type == TOKEN_TRUE ? 1.0 : 0.0;
        advance_token();
        uint64_t bits;
        memcpy(&bits, &v, sizeof(bits));
        emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
        emit_push_reg(code, REG_RBX);
        emit_mov_reg_imm64(code, REG_RAX, bits);
        emit_push_reg(code, REG_RAX);
        return;
    }
    if (current.type == TOKEN_NIL) {
        advance_token();
        emit_mov_reg_imm64(code, REG_RBX, TAG_NIL);
        emit_push_reg(code, REG_RBX);
        emit_mov_reg_imm64(code, REG_RAX, 0);
        emit_push_reg(code, REG_RAX);
        return;
    }
    if (current.type == TOKEN_IDENTIFIER) {
        Token id = current;
        advance_token();

        if (current.type == TOKEN_DOT) {
            advance_token();
            expect(TOKEN_IDENTIFIER, "expected name after '.'");
            Token member = previous;

            char combined[128];
            int combined_len = snprintf(combined, sizeof(combined), "%.*s.%.*s",
                                          id.length, id.start, member.length, member.start);

            if (current.type == TOKEN_LPAREN) {
                advance_token();
                codegen_call_named(combined, combined_len);
                return;
            }

            KilnConstant *k = resolve_constant(combined, combined_len);
            if (k != NULL) { codegen_constant_value(k); return; }

            parse_error("undefined namespaced function or constant");
        }

        if (current.type == TOKEN_LPAREN) {
            advance_token();
            codegen_call(id);
            return;
        }
        int slot = resolve_var(id.start, id.length);
        if (slot != -1) {
            emit_load_mem_disp32(code, REG_RBX, REG_RBP, var_slot_tag_offset(slot));
            emit_push_reg(code, REG_RBX);
            emit_load_mem_disp32(code, REG_RAX, REG_RBP, var_slot_payload_offset(slot));
            emit_push_reg(code, REG_RAX);
            return;
        }
        // Not a variable -- fall back to a bare named-function reference
        // used as a value (not immediately called), wrapped as a
        // zero-capture closure (see codegen/C/functions/closures.c).
        KilnFunction *fn = resolve_function_in_context(id.start, id.length);
        if (fn != NULL) {
            codegen_named_function_value(fn);
            return;
        }
        // Not a function either -- if we're compiling inside an imported
        // file's own namespace context, a bare reference may be that same
        // file's own top-level constant: let_statement always registers
        // imported `let`s under their namespaced name (e.g. "lib.PI"),
        // even for references from within lib.ash itself, so a bare "PI"
        // used inside lib.ash needs this same "in context" namespacing
        // resolve_function_in_context already does for bare function
        // names above -- otherwise a constant could never be used from
        // within the file that declares it.
        const char *ns; int ns_len;
        get_import_namespace(&ns, &ns_len);
        if (ns_len > 0) {
            char combined[128];
            int combined_len = snprintf(combined, sizeof(combined), "%.*s.%.*s", ns_len, ns, id.length, id.start);
            KilnConstant *k = resolve_constant(combined, combined_len);
            if (k != NULL) { codegen_constant_value(k); return; }
        }
        char msg[128];
        snprintf(msg, sizeof(msg), "undefined variable: %.*s", id.length, id.start);
        parse_error(msg);
    }
    if (current.type == TOKEN_LPAREN) {
        advance_token();
        codegen_expression();
        expect(TOKEN_RPAREN, "expected ')' after expression");
        return;
    }
    if (current.type == TOKEN_LBRACKET) {
        codegen_array_literal();
        return;
    }
    if (current.type == TOKEN_LBRACE) {
        codegen_map_literal();
        return;
    }
    parse_error("expected a number, string, array, map, variable, or '('");
}

// `x[index]` reads -- arrays and maps both use this syntax, and which one
// `x` actually is can only be known at runtime, so this pops both values
// just far enough to check the base's tag, pushes them straight back in
// the same order, and dispatches. A while loop (not a single check) so
// chains like `arr[0][1]` work, matching ashvm/src/compiler/expr.c's
// postfix() shape.
static void postfix(void) {
    primary();
    while (current.type == TOKEN_LBRACKET) {
        advance_token();
        codegen_expression();
        expect(TOKEN_RBRACKET, "expected ']' after index");

        emit_pop_reg(code, REG_RAX); // index payload
        emit_pop_reg(code, REG_RBX); // index tag
        emit_pop_reg(code, REG_RCX); // base payload
        emit_pop_reg(code, REG_RDX); // base tag
        emit_push_reg(code, REG_RDX);
        emit_push_reg(code, REG_RCX);
        emit_push_reg(code, REG_RBX);
        emit_push_reg(code, REG_RAX);
        emit_cmp_reg_imm32(code, REG_RDX, TAG_MAP);
        int is_map = emit_jcc_rel32(code, COND_E);
        emit_cmp_reg_imm32(code, REG_RDX, TAG_STRING);
        int is_string = emit_jcc_rel32(code, COND_E);
        codegen_array_index_read();
        int done = emit_jmp_rel32(code);
        emit_patch_jump(code, is_map);
        codegen_map_index_read();
        int done2 = emit_jmp_rel32(code);
        emit_patch_jump(code, is_string);
        codegen_string_index_read();
        emit_patch_jump(code, done);
        emit_patch_jump(code, done2);
    }
}

static void unary(void) {
    // Kiln-vs-ashvm parity gap (missing.md): ashvm implements this
    // (TOKEN_BANG -> OP_NOT); kiln never wired it up. Logical not: pushes
    // 1.0 if the operand's payload is zero, else 0.0 -- matches
    // pop_and_test_truthy's "NUMBER != 0" definition of truthy elsewhere
    // in this file, and works for nil too since nil's payload is always
    // exactly 0 (see value.h's TAG_NIL comment).
    if (current.type == TOKEN_BANG) {
        advance_token();
        unary();
        emit_pop_reg(code, REG_RAX); // payload
        emit_pop_reg(code, REG_RBX); // tag (ignored)
        emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
        emit_pxor_xmm_xmm(code, XMM1);
        emit_ucomisd(code, XMM0, XMM1);
        int is_zero = emit_jcc_rel32(code, COND_E);
        emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
        emit_push_reg(code, REG_RBX);
        emit_pxor_xmm_xmm(code, XMM0);
        emit_movq_reg_from_xmm(code, REG_RAX, XMM0);
        emit_push_reg(code, REG_RAX); // nonzero operand -> 0.0
        int done = emit_jmp_rel32(code);
        emit_patch_jump(code, is_zero);
        emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
        emit_push_reg(code, REG_RBX);
        double one = 1.0;
        uint64_t one_bits;
        memcpy(&one_bits, &one, sizeof(one_bits));
        emit_mov_reg_imm64(code, REG_RAX, one_bits);
        emit_push_reg(code, REG_RAX); // zero operand -> 1.0
        emit_patch_jump(code, done);
        return;
    }
    if (current.type == TOKEN_MINUS) {
        advance_token();
        unary();
        emit_pop_reg(code, REG_RAX); // payload
        emit_pop_reg(code, REG_RBX); // tag (assumed NUMBER)
        emit_movq_xmm_from_reg(code, XMM1, REG_RAX);
        emit_pxor_xmm_xmm(code, XMM0);
        emit_subsd(code, XMM0, XMM1); // XMM0 = 0.0 - operand
        emit_movq_reg_from_xmm(code, REG_RAX, XMM0);
        emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
        emit_push_reg(code, REG_RBX);
        emit_push_reg(code, REG_RAX);
        return;
    }
    if (current.type == TOKEN_TILDE) {
        advance_token();
        unary();
        emit_pop_reg(code, REG_RAX); // payload
        emit_pop_reg(code, REG_RBX); // tag (assumed NUMBER)
        emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
        emit_cvttsd2si(code, REG_RAX, XMM0); // -> int64
        emit_not_reg(code, REG_RAX);
        emit_cvtsi2sd(code, XMM0, REG_RAX);
        emit_movq_reg_from_xmm(code, REG_RAX, XMM0);
        emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
        emit_push_reg(code, REG_RBX);
        emit_push_reg(code, REG_RAX);
        return;
    }
    power();
}

// x ** y  -- sits between unary and postfix, so `-2 ** 2` parses as
// `-(2 ** 2)` (Python's convention) and `2 ** 3 ** 2` is right-associative
// (the exponent is parsed via unary(), not power(), so it recurses back
// through power() for the next `**`): `2 ** 3 ** 2` = `2 ** (3 ** 2)`.
//
// Integer exponents only (documented simplification, not silently wrong:
// the exponent is truncated via cvttsd2si same as the bitwise operators
// truncate their operands) -- computed by exponentiation-by-squaring, a
// genuine runtime loop since the exponent is only known at runtime,
// unlike every other arithmetic operator here. Negative exponents work
// via computing on the absolute value then reciprocating. No x87
// FYL2X/F2XM1 in this project (SSE2 only), so fractional exponents
// (x ** 0.5) aren't supported -- that would need a real pow(), out of
// scope here.
static void power(void) {
    postfix();
    if (current.type == TOKEN_STAR_STAR) {
        advance_token();
        unary(); // exponent -- right-associative, allows `2 ** -3`

        emit_pop_reg(code, REG_RAX); // exponent payload
        emit_pop_reg(code, REG_RBX); // exponent tag (ignored)
        emit_pop_reg(code, REG_RCX); // base payload
        emit_pop_reg(code, REG_RDX); // base tag (ignored)

        emit_movq_xmm_from_reg(code, XMM0, REG_RCX); // base
        emit_movq_xmm_from_reg(code, XMM2, REG_RAX); // exponent (double), temp
        emit_cvttsd2si(code, REG_RCX, XMM2); // n = truncate(exponent)

        emit_mov_reg_imm64(code, REG_RBX, 0); // neg flag
        emit_cmp_reg_imm32(code, REG_RCX, 0);
        int not_neg = emit_jcc_rel32(code, COND_GE);
        emit_mov_reg_imm64(code, REG_RBX, 1);
        emit_neg_reg(code, REG_RCX); // n = |n|
        emit_patch_jump(code, not_neg);

        double one = 1.0;
        uint64_t one_bits;
        memcpy(&one_bits, &one, sizeof(one_bits));
        emit_mov_reg_imm64(code, REG_RAX, one_bits);
        emit_movq_xmm_from_reg(code, XMM1, REG_RAX); // result = 1.0

        int loop_start = code->count;
        emit_cmp_reg_imm32(code, REG_RCX, 0);
        int loop_exit = emit_jcc_rel32(code, COND_LE);

        emit_mov_reg_reg(code, REG_RAX, REG_RCX);
        emit_mov_reg_imm64(code, REG_RDX, 1);
        emit_and_reg_reg(code, REG_RAX, REG_RDX); // RAX = n & 1
        emit_cmp_reg_imm32(code, REG_RAX, 0);
        int skip_mul = emit_jcc_rel32(code, COND_E);
        emit_mulsd(code, XMM1, XMM0); // result *= base
        emit_patch_jump(code, skip_mul);

        emit_mulsd(code, XMM0, XMM0); // base *= base
        emit_sar_reg_by1(code, REG_RCX); // n >>= 1
        emit_jmp_back(code, loop_start);
        emit_patch_jump(code, loop_exit);

        emit_cmp_reg_imm32(code, REG_RBX, 0);
        int not_negative = emit_jcc_rel32(code, COND_E);
        emit_mov_reg_imm64(code, REG_RAX, one_bits);
        emit_movq_xmm_from_reg(code, XMM2, REG_RAX);
        emit_divsd(code, XMM2, XMM1); // XMM2 = 1.0 / result
        emit_movsd_xmm_xmm(code, XMM1, XMM2);
        emit_patch_jump(code, not_negative);

        emit_movq_reg_from_xmm(code, REG_RAX, XMM1);
        emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
        emit_push_reg(code, REG_RBX);
        emit_push_reg(code, REG_RAX);
    }
}

static void term(void) {
    unary();
    while (current.type == TOKEN_STAR || current.type == TOKEN_SLASH) {
        TokenType op = current.type;
        advance_token();
        unary();
        emit_pop_reg(code, REG_RCX); // b payload
        emit_pop_reg(code, REG_RDX); // b tag (assumed NUMBER)
        emit_pop_reg(code, REG_RAX); // a payload
        emit_pop_reg(code, REG_RBX); // a tag (assumed NUMBER)
        emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
        emit_movq_xmm_from_reg(code, XMM1, REG_RCX);
        if (op == TOKEN_STAR) emit_mulsd(code, XMM0, XMM1);
        else emit_divsd(code, XMM0, XMM1);
        emit_movq_reg_from_xmm(code, REG_RAX, XMM0);
        emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
        emit_push_reg(code, REG_RBX);
        emit_push_reg(code, REG_RAX);
    }
}

// See expr.h's header comment -- shared by additive()'s `+` and
// parser.c's `x += e` compound assignment.
void codegen_apply_plus(void) {
    // Runtime tag check: both STRING -> concatenate, else assume both
    // NUMBER (see the scope-limit note at the top of this file).
    emit_cmp_reg_imm32(code, REG_RBX, TAG_STRING);
    int not_string = emit_jcc_rel32(code, COND_NE);
    emit_cmp_reg_imm32(code, REG_RDX, TAG_STRING);
    int not_string2 = emit_jcc_rel32(code, COND_NE);
    codegen_string_concat(); // consumes a_payload=RAX, b_payload=RCX; pushes its own result
    int done = emit_jmp_rel32(code);
    emit_patch_jump(code, not_string);
    emit_patch_jump(code, not_string2);
    emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
    emit_movq_xmm_from_reg(code, XMM1, REG_RCX);
    emit_addsd(code, XMM0, XMM1);
    emit_movq_reg_from_xmm(code, REG_RAX, XMM0);
    emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
    emit_patch_jump(code, done);
}

void additive(void) {
    term();
    while (current.type == TOKEN_PLUS || current.type == TOKEN_MINUS) {
        TokenType op = current.type;
        advance_token();
        term();
        emit_pop_reg(code, REG_RCX); // b payload
        emit_pop_reg(code, REG_RDX); // b tag
        emit_pop_reg(code, REG_RAX); // a payload
        emit_pop_reg(code, REG_RBX); // a tag

        if (op == TOKEN_PLUS) {
            codegen_apply_plus();
        } else {
            emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
            emit_movq_xmm_from_reg(code, XMM1, REG_RCX);
            emit_subsd(code, XMM0, XMM1);
            emit_movq_reg_from_xmm(code, REG_RAX, XMM0);
            emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
            emit_push_reg(code, REG_RBX);
            emit_push_reg(code, REG_RAX);
        }
    }
}
