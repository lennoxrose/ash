#include "codegen/H/builtins/math_builtins.h"
#include "codegen/H/emit/emit.h"
#include "codegen/H/emit/emit_sse.h"
#include "codegen/H/emit/value.h"
#include "parser/H/core/parser.h"

void codegen_builtin_sqrt(void) {
    emit_pop_reg(code, REG_RAX); // payload
    emit_pop_reg(code, REG_RBX); // tag (assumed NUMBER)
    emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
    emit_sqrtsd(code, XMM0, XMM0);
    emit_movq_reg_from_xmm(code, REG_RAX, XMM0);
    emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}

void codegen_builtin_abs(void) {
    emit_pop_reg(code, REG_RAX); // payload
    emit_pop_reg(code, REG_RBX); // tag (assumed NUMBER)
    emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
    emit_mov_reg_imm64(code, REG_RAX, 0x7FFFFFFFFFFFFFFFULL); // clears just the sign bit
    emit_movq_xmm_from_reg(code, XMM1, REG_RAX);
    emit_andpd(code, XMM0, XMM1);
    emit_movq_reg_from_xmm(code, REG_RAX, XMM0);
    emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}

// cvttsd2si truncates TOWARD ZERO, which is already floor() for x>=0 but
// rounds the wrong way for a negative x with a fractional part (e.g.
// -3.5 truncates to -3, but floor(-3.5) is -4) -- converting the
// truncated result back to a double and checking whether it OVERSHOT the
// original value catches exactly that case.
void codegen_builtin_floor(void) {
    emit_pop_reg(code, REG_RAX); // payload
    emit_pop_reg(code, REG_RBX); // tag (assumed NUMBER)
    emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
    emit_cvttsd2si(code, REG_RCX, XMM0);
    emit_cvtsi2sd(code, XMM1, REG_RCX);
    emit_ucomisd(code, XMM1, XMM0);
    int no_overshoot = emit_jcc_rel32(code, COND_BE); // truncated <= original -> already floor
    emit_dec_reg(code, REG_RCX);
    emit_patch_jump(code, no_overshoot);
    emit_cvtsi2sd(code, XMM0, REG_RCX);
    emit_movq_reg_from_xmm(code, REG_RAX, XMM0);
    emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}
