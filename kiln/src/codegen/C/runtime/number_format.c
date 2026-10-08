#include <string.h>
#include "codegen/H/runtime/number_format.h"
#include "codegen/H/emit/emit_internal.h"
#include "codegen/H/emit/emit_sse.h"

static void load_double_const(CodeBuf *code, XReg dst, double v) {
    uint64_t bits;
    memcpy(&bits, &v, sizeof(bits));
    emit_mov_reg_imm64(code, REG_RAX, bits);
    emit_movq_xmm_from_reg(code, dst, REG_RAX);
}

// Writes `text` at [RCX...] advancing RCX, for the NaN/inf literals.
static void store_text(CodeBuf *code, const char *text) {
    for (const char *p = text; *p; p++) {
        emit_store_byte_imm(code, REG_RCX, (uint8_t)*p);
        emit_add_reg_imm8(code, REG_RCX, 1);
    }
}

// Scratch layout (relative to RSP after the sub; kiln's emitter has no
// [rsp+disp] addressing, so everything is reached through pointer registers):
//   [0, 32)   sign + integer digits, built backward from 32
//   32        '.'
//   [33, 39)  up to 6 fraction digits, built forward
//   39...     exponent text ('e', sign, digits) in scientific mode (<= 6 bytes)
// Register roles: RBX = sign, then frac_len; RDI = the decimal exponent in
// scientific mode, or NF_PLAIN in plain mode.
#define NF_PLAIN 0x7FFFFFFF
void number_format_emit(CodeBuf *code) {
    emit_sub_reg_imm8(code, REG_RSP, NUMBER_FORMAT_SCRATCH);
    emit_movq_xmm_from_reg(code, XMM0, REG_RAX);

    // ---- NaN / infinity: exponent bits all ones ----
    emit_mov_reg_imm64(code, REG_RCX, 0x7FF0000000000000ull);
    emit_mov_reg_reg(code, REG_RDX, REG_RAX);
    emit_and_reg_reg(code, REG_RDX, REG_RCX);
    emit_cmp_reg_reg(code, REG_RDX, REG_RCX);
    int finite = emit_jcc_rel32(code, COND_NE);

    emit_mov_reg_imm64(code, REG_RCX, 0x000FFFFFFFFFFFFFull);
    emit_and_reg_reg(code, REG_RCX, REG_RAX);
    emit_cmp_reg_imm32(code, REG_RCX, 0);
    int is_inf = emit_jcc_rel32(code, COND_E);
    emit_mov_reg_reg(code, REG_RCX, REG_RSP);
    store_text(code, "nan");
    int special_done_nan = emit_jmp_rel32(code);

    emit_patch_jump(code, is_inf);
    emit_mov_reg_reg(code, REG_RCX, REG_RSP);
    emit_pxor_xmm_xmm(code, XMM2);
    emit_ucomisd(code, XMM0, XMM2);
    int inf_pos = emit_jcc_rel32(code, COND_AE);
    store_text(code, "-");
    emit_patch_jump(code, inf_pos);
    store_text(code, "inf");
    emit_patch_jump(code, special_done_nan);
    emit_mov_reg_reg(code, REG_RDX, REG_RCX); // end
    emit_mov_reg_reg(code, REG_RCX, REG_RSP); // start
    int special_done = emit_jmp_rel32(code);

    emit_patch_jump(code, finite);

    // ---- sign: RBX = 1 if negative, XMM0 = |x| ----
    emit_pxor_xmm_xmm(code, XMM2);
    emit_ucomisd(code, XMM0, XMM2);
    emit_mov_reg_imm64(code, REG_RBX, 0);
    int skip_negate = emit_jcc_rel32(code, COND_AE);
    emit_subsd(code, XMM2, XMM0);
    emit_movsd_xmm_xmm(code, XMM0, XMM2);
    emit_mov_reg_imm64(code, REG_RBX, 1);
    emit_patch_jump(code, skip_negate);

    // ---- plain (0 or 1e-6 <= x < 1e15) vs scientific ----
    emit_mov_reg_imm64(code, REG_RDI, NF_PLAIN);
    emit_pxor_xmm_xmm(code, XMM2);
    emit_ucomisd(code, XMM0, XMM2);
    int plain = emit_jcc_rel32(code, COND_E); // zero
    load_double_const(code, XMM1, 1e15);
    emit_ucomisd(code, XMM0, XMM1);
    int sci = emit_jcc_rel32(code, COND_AE);
    load_double_const(code, XMM1, 1e-6);
    emit_ucomisd(code, XMM0, XMM1);
    int sci_small = emit_jcc_rel32(code, COND_B);

    emit_patch_jump(code, plain);
    // plain: nudge for round-half-up (carry reaches the integer part), then split
    load_double_const(code, XMM1, 0.0000005);
    emit_addsd(code, XMM0, XMM1);
    emit_cvttsd2si(code, REG_RAX, XMM0);
    emit_cvtsi2sd(code, XMM1, REG_RAX);
    emit_subsd(code, XMM0, XMM1);
    int to_digits = emit_jmp_rel32(code);

    // scientific: normalize XMM0 into [1,10), counting the exponent in RDI
    emit_patch_jump(code, sci);
    emit_patch_jump(code, sci_small);
    emit_mov_reg_imm64(code, REG_RDI, 0);
    load_double_const(code, XMM3, 10.0);
    load_double_const(code, XMM1, 1.0);
    emit_ucomisd(code, XMM0, XMM1);
    int too_big = emit_jcc_rel32(code, COND_AE);
    int small_loop = code->count;
    emit_ucomisd(code, XMM0, XMM1);
    int normalized_small = emit_jcc_rel32(code, COND_AE);
    emit_mulsd(code, XMM0, XMM3);
    emit_sub_reg_imm8(code, REG_RDI, 1);
    emit_jmp_back(code, small_loop);
    emit_patch_jump(code, too_big);
    int big_loop = code->count;
    emit_ucomisd(code, XMM0, XMM3);
    int normalized_big = emit_jcc_rel32(code, COND_B);
    emit_divsd(code, XMM0, XMM3);
    emit_add_reg_imm8(code, REG_RDI, 1);
    emit_jmp_back(code, big_loop);
    emit_patch_jump(code, normalized_small);
    emit_patch_jump(code, normalized_big);
    // round the mantissa; 9.9999996 carries to 10 -> 1 and exponent + 1
    load_double_const(code, XMM2, 0.0000005);
    emit_addsd(code, XMM0, XMM2);
    emit_ucomisd(code, XMM0, XMM3);
    int no_carry = emit_jcc_rel32(code, COND_B);
    emit_movsd_xmm_xmm(code, XMM0, XMM1); // 1.0
    emit_add_reg_imm8(code, REG_RDI, 1);
    emit_patch_jump(code, no_carry);
    emit_cvttsd2si(code, REG_RAX, XMM0);
    emit_cvtsi2sd(code, XMM1, REG_RAX);
    emit_subsd(code, XMM0, XMM1);

    // ---- integer digits (RAX), backward ----
    emit_patch_jump(code, to_digits);
    emit_mov_reg_reg(code, REG_RCX, REG_RSP);
    emit_add_reg_imm8(code, REG_RCX, 32);
    int loop_start = code->count;
    emit_dec_reg(code, REG_RCX);
    emit_cqo(code);
    emit_mov_reg_imm64(code, REG_RSI, 10);
    emit_idiv_reg(code, REG_RSI);
    emit_add_reg_imm8(code, REG_RDX, '0');
    emit_store_byte_reg(code, REG_RCX, REG_RDX);
    emit_cmp_reg_imm32(code, REG_RAX, 0);
    emit_jcc_back(code, COND_NE, loop_start);

    emit_cmp_reg_imm32(code, REG_RBX, 0);
    int skip_sign = emit_jcc_rel32(code, COND_E);
    emit_dec_reg(code, REG_RCX);
    emit_store_byte_imm(code, REG_RCX, '-');
    emit_patch_jump(code, skip_sign);

    // ---- fraction digits, forward; RBX = how many are significant ----
    emit_mov_reg_reg(code, REG_RSI, REG_RSP);
    emit_add_reg_imm8(code, REG_RSI, 33);
    load_double_const(code, XMM3, 10.0);
    emit_mov_reg_imm64(code, REG_RBX, 0);
    for (int i = 0; i < 6; i++) {
        emit_mulsd(code, XMM0, XMM3);
        emit_cvttsd2si(code, REG_RAX, XMM0);
        emit_cvtsi2sd(code, XMM1, REG_RAX);
        emit_subsd(code, XMM0, XMM1);
        emit_cmp_reg_imm32(code, REG_RAX, 0);
        int skip_mark = emit_jcc_rel32(code, COND_E);
        emit_mov_reg_imm64(code, REG_RBX, (uint64_t)(i + 1));
        emit_patch_jump(code, skip_mark);
        emit_add_reg_imm8(code, REG_RAX, '0');
        emit_store_byte_reg(code, REG_RSI, REG_RAX);
        emit_add_reg_imm8(code, REG_RSI, 1);
    }

    // ---- '.' placement: RSI = end of text ----
    emit_mov_reg_reg(code, REG_RSI, REG_RSP);
    emit_add_reg_imm8(code, REG_RSI, 32);
    emit_cmp_reg_imm32(code, REG_RBX, 0);
    int whole = emit_jcc_rel32(code, COND_E);
    emit_mov_reg_reg(code, REG_RDX, REG_RSP);
    emit_add_reg_imm8(code, REG_RDX, 32);
    emit_store_byte_imm(code, REG_RDX, '.');
    emit_add_reg_imm8(code, REG_RSI, 1);
    emit_add_reg_reg(code, REG_RSI, REG_RBX);
    emit_patch_jump(code, whole);

    // ---- exponent suffix (scientific mode only) ----
    emit_cmp_reg_imm32(code, REG_RDI, NF_PLAIN);
    int no_exp = emit_jcc_rel32(code, COND_E);
    emit_store_byte_imm(code, REG_RSI, 'e');
    emit_add_reg_imm8(code, REG_RSI, 1);
    emit_mov_reg_reg(code, REG_RAX, REG_RDI);
    emit_cmp_reg_imm32(code, REG_RAX, 0);
    int exp_neg = emit_jcc_rel32(code, COND_LT);
    emit_store_byte_imm(code, REG_RSI, '+');
    int sign_done = emit_jmp_rel32(code);
    emit_patch_jump(code, exp_neg);
    emit_store_byte_imm(code, REG_RSI, '-');
    emit_mov_reg_imm64(code, REG_RDX, 0);
    emit_sub_reg_reg(code, REG_RDX, REG_RAX);
    emit_mov_reg_reg(code, REG_RAX, REG_RDX); // |e|
    emit_patch_jump(code, sign_done);
    emit_add_reg_imm8(code, REG_RSI, 1);
    // hundreds digit only when |e| >= 100
    emit_cmp_reg_imm32(code, REG_RAX, 100);
    int two_digits = emit_jcc_rel32(code, COND_LT);
    emit_cqo(code);
    emit_mov_reg_imm64(code, REG_RBX, 100);
    emit_idiv_reg(code, REG_RBX);               // RAX = hundreds, RDX = rest
    emit_add_reg_imm8(code, REG_RAX, '0');
    emit_store_byte_reg(code, REG_RSI, REG_RAX);
    emit_add_reg_imm8(code, REG_RSI, 1);
    emit_mov_reg_reg(code, REG_RAX, REG_RDX);
    emit_patch_jump(code, two_digits);
    emit_cqo(code);
    emit_mov_reg_imm64(code, REG_RBX, 10);
    emit_idiv_reg(code, REG_RBX);               // RAX = tens, RDX = ones
    emit_add_reg_imm8(code, REG_RAX, '0');
    emit_store_byte_reg(code, REG_RSI, REG_RAX);
    emit_add_reg_imm8(code, REG_RSI, 1);
    emit_add_reg_imm8(code, REG_RDX, '0');
    emit_store_byte_reg(code, REG_RSI, REG_RDX);
    emit_add_reg_imm8(code, REG_RSI, 1);
    emit_patch_jump(code, no_exp);

    emit_mov_reg_reg(code, REG_RDX, REG_RSI); // end of text (RCX = start)
    emit_patch_jump(code, special_done);
}
