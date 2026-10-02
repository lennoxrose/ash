#include <string.h>
#include "codegen/expr.h"
#include "codegen/expr_internal.h"
#include "codegen/emit.h"
#include "codegen/emit_sse.h"
#include "codegen/value.h"
#include "codegen/strings.h"
#include "parser/parser.h"

// Precedence climbing continued from expr.c: comparison -> logical_and ->
// logical_or -> expression. See expr.c's header comment for the full
// chain, the (tag, payload) push/pop convention, and the scope-limit
// note (only `+` and `==`/`!=` are tag-aware; the rest assume NUMBER).

// Pushes a literal double 0.0 or 1.0, tagged NUMBER.
static void push_number_literal(double v) {
    emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
    emit_push_reg(code, REG_RBX);
    if (v == 0.0) {
        emit_pxor_xmm_xmm(code, XMM0);
        emit_movq_reg_from_xmm(code, REG_RAX, XMM0);
    } else {
        uint64_t bits;
        memcpy(&bits, &v, sizeof(bits));
        emit_mov_reg_imm64(code, REG_RAX, bits);
    }
    emit_push_reg(code, REG_RAX);
}

// Turns "condition true/false" (as of the last ucomisd) into a clean
// pushed 0.0/1.0, via the same jcc-and-patch technique
// codegen/print_int.c already uses for its digit loop -- just applied to
// a general boolean result instead of a hardcoded loop exit. `cc_if_true`
// must be one of ucomisd's unsigned-style condition codes (COND_B/BE/A/AE
// or COND_E/NE), never the signed integer ones.
static void push_bool(Cond cc_if_true) {
    int to_true = emit_jcc_rel32(code, cc_if_true);
    push_number_literal(0.0);
    int to_end = emit_jmp_rel32(code);
    emit_patch_jump(code, to_true);
    push_number_literal(1.0);
    emit_patch_jump(code, to_end);
}

// Pops the top-of-stack value and sets flags as if its payload were
// compared against 0.0 (ucomisd), for truthiness tests -- assumes
// NUMBER, matching ashvm's own truthy(), which requires a number too.
static void pop_and_test_truthy(void) {
    emit_pop_reg(code, REG_RAX);  // payload
    emit_pop_reg(code, REG_RBX);  // tag (assumed NUMBER)
    emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
    emit_pxor_xmm_xmm(code, XMM1);
    emit_ucomisd(code, XMM0, XMM1);
}

// Not chainable (`a < b < c` isn't a single comparison), matching
// ashvm/src/compiler/expr.c's comparison() shape exactly (an `if`, not a
// `while`).
static void comparison(void) {
    additive();
    if (current.type == TOKEN_EQUAL_EQUAL || current.type == TOKEN_BANG_EQUAL ||
        current.type == TOKEN_LESS || current.type == TOKEN_LESS_EQUAL ||
        current.type == TOKEN_GREATER || current.type == TOKEN_GREATER_EQUAL) {
        TokenType op = current.type;
        advance_token();
        additive();
        emit_pop_reg(code, REG_RCX); // b payload
        emit_pop_reg(code, REG_RDX); // b tag
        emit_pop_reg(code, REG_RAX); // a payload
        emit_pop_reg(code, REG_RBX); // a tag

        if (op == TOKEN_EQUAL_EQUAL || op == TOKEN_BANG_EQUAL) {
            int invert = (op == TOKEN_BANG_EQUAL);
            // Different tags can never be equal (matches ashvm's do_EQ:
            // mismatched types just aren't equal, no error).
            emit_cmp_reg_reg(code, REG_RBX, REG_RDX);
            int tags_differ = emit_jcc_rel32(code, COND_NE);
            emit_cmp_reg_imm32(code, REG_RBX, TAG_STRING);
            int not_string = emit_jcc_rel32(code, COND_NE);
            codegen_string_compare(invert); // consumes a_payload=RAX, b_payload=RCX; pushes its own result
            int done = emit_jmp_rel32(code);
            emit_patch_jump(code, not_string);
            emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
            emit_movq_xmm_from_reg(code, XMM1, REG_RCX);
            emit_ucomisd(code, XMM0, XMM1);
            push_bool(invert ? COND_NE : COND_E);
            int done2 = emit_jmp_rel32(code);
            emit_patch_jump(code, tags_differ);
            push_number_literal(invert ? 1.0 : 0.0);
            emit_patch_jump(code, done);
            emit_patch_jump(code, done2);
        } else {
            emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
            emit_movq_xmm_from_reg(code, XMM1, REG_RCX);
            emit_ucomisd(code, XMM0, XMM1);
            switch (op) {
                case TOKEN_LESS:          push_bool(COND_B);  break;
                case TOKEN_LESS_EQUAL:    push_bool(COND_BE); break;
                case TOKEN_GREATER:       push_bool(COND_A);  break;
                case TOKEN_GREATER_EQUAL: push_bool(COND_AE); break;
                default: break;
            }
        }
    }
}

// Bitwise operators (& | ^ << >>), one flat precedence level (left to
// right) sitting between comparison and logical_and -- not real C
// precedence (which splits shift/bitand/bitxor/bitor into four separate
// levels), a deliberate simplification since mixed bitwise+comparison
// expressions are rare enough that parens can disambiguate when needed.
// Operands are truncated to int64 (cvttsd2si), same "assume NUMBER"
// scope limit as -, *, /, and the comparison operators.
static void bitwise(void) {
    comparison();
    while (current.type == TOKEN_AMP || current.type == TOKEN_PIPE ||
           current.type == TOKEN_CARET || current.type == TOKEN_SHL || current.type == TOKEN_SHR) {
        TokenType op = current.type;
        advance_token();
        comparison();
        emit_pop_reg(code, REG_RCX); // b payload
        emit_pop_reg(code, REG_RDX); // b tag (ignored)
        emit_pop_reg(code, REG_RAX); // a payload
        emit_pop_reg(code, REG_RBX); // a tag (ignored)

        emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
        emit_cvttsd2si(code, REG_RAX, XMM0); // a -> int64
        emit_movq_xmm_from_reg(code, XMM0, REG_RCX);
        emit_cvttsd2si(code, REG_RCX, XMM0); // b -> int64

        switch (op) {
            case TOKEN_AMP:   emit_and_reg_reg(code, REG_RAX, REG_RCX); break;
            case TOKEN_PIPE:  emit_or_reg_reg(code, REG_RAX, REG_RCX); break;
            case TOKEN_CARET: emit_xor_reg_reg(code, REG_RAX, REG_RCX); break;
            case TOKEN_SHL:   emit_shl_reg_cl(code, REG_RAX); break;
            case TOKEN_SHR:   emit_sar_reg_cl(code, REG_RAX); break;
            default: break;
        }

        emit_cvtsi2sd(code, XMM0, REG_RAX);
        emit_movq_reg_from_xmm(code, REG_RAX, XMM0);
        emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
        emit_push_reg(code, REG_RBX);
        emit_push_reg(code, REG_RAX);
    }
}

// Short-circuit &&: if the left side is falsy, the right side never gets
// evaluated (its code doesn't even run, not just "result ignored").
static void logical_and(void) {
    bitwise();
    while (current.type == TOKEN_AND) {
        advance_token();
        pop_and_test_truthy();
        int to_false = emit_jcc_rel32(code, COND_E);
        bitwise();
        pop_and_test_truthy();
        int to_false2 = emit_jcc_rel32(code, COND_E);
        push_number_literal(1.0);
        int to_end = emit_jmp_rel32(code);
        emit_patch_jump(code, to_false);
        emit_patch_jump(code, to_false2);
        push_number_literal(0.0);
        emit_patch_jump(code, to_end);
    }
}

// Short-circuit ||: if the left side is truthy, the right side never gets
// evaluated.
static void logical_or(void) {
    logical_and();
    while (current.type == TOKEN_OR) {
        advance_token();
        pop_and_test_truthy();
        int to_true = emit_jcc_rel32(code, COND_NE);
        logical_and();
        pop_and_test_truthy();
        int to_true2 = emit_jcc_rel32(code, COND_NE);
        push_number_literal(0.0);
        int to_end = emit_jmp_rel32(code);
        emit_patch_jump(code, to_true);
        emit_patch_jump(code, to_true2);
        push_number_literal(1.0);
        emit_patch_jump(code, to_end);
    }
}

// a ? b : c -- lowest precedence, sits above logical_or. Right-associative
// via the recursive codegen_expression() calls for both branches (so
// `a ? b : c ? d : e` parses as `a ? b : (c ? d : e)`), same shape as
// if/else's truthy-test-then-jump pattern, just as an expression instead
// of a statement.
void codegen_expression(void) {
    logical_or();
    if (current.type == TOKEN_QUESTION) {
        advance_token();
        emit_pop_reg(code, REG_RAX); // condition payload
        emit_pop_reg(code, REG_RBX); // condition tag (assumed NUMBER)
        emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
        emit_pxor_xmm_xmm(code, XMM1);
        emit_ucomisd(code, XMM0, XMM1);
        int else_jump = emit_jcc_rel32(code, COND_E);

        codegen_expression(); // true branch
        int end_jump = emit_jmp_rel32(code);
        expect(TOKEN_COLON, "expected ':' in ternary expression");
        emit_patch_jump(code, else_jump);
        codegen_expression(); // false branch
        emit_patch_jump(code, end_jump);
    }
}
