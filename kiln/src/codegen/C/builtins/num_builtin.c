#include <string.h>
#include "codegen/H/builtins/convert_builtins.h"
#include "codegen/H/emit/emit.h"
#include "codegen/H/emit/emit_sse.h"
#include "codegen/H/emit/value.h"
#include "parser/H/core/parser.h"

static void load_double_const(XReg dst, double v) {
    uint64_t bits;
    memcpy(&bits, &v, sizeof(bits));
    emit_mov_reg_imm64(code, REG_RAX, bits);
    emit_movq_xmm_from_reg(code, dst, REG_RAX);
}

// num(string): RSI scans the string's bytes, RDI is the fixed end
// pointer (payload+length) -- no libc strtod in this freestanding
// binary, so this hand-parses the same grammar kiln's own number
// literals accept (plus a leading sign and leading whitespace, matching
// strtod). Stops cleanly at the first byte it can't consume.
void codegen_builtin_num(void) {
    emit_pop_reg(code, REG_RSI); // string payload address
    emit_pop_reg(code, REG_RBX); // tag (assumed STRING)

    emit_load_mem_disp32(code, REG_RDI, REG_RSI, -8); // length
    emit_add_reg_reg(code, REG_RDI, REG_RSI);          // RDI = end pointer

    // ---- skip leading whitespace ----
    int ws_loop = code->count;
    emit_cmp_reg_reg(code, REG_RSI, REG_RDI);
    int ws_done = emit_jcc_rel32(code, COND_GE);
    emit_load_byte_reg(code, REG_RCX, REG_RSI);
    emit_cmp_reg_imm32(code, REG_RCX, ' ');
    int not_space = emit_jcc_rel32(code, COND_NE);
    emit_add_reg_imm8(code, REG_RSI, 1);
    emit_jmp_back(code, ws_loop);
    emit_patch_jump(code, not_space);
    emit_cmp_reg_imm32(code, REG_RCX, '\t');
    int not_tab = emit_jcc_rel32(code, COND_NE);
    emit_add_reg_imm8(code, REG_RSI, 1);
    emit_jmp_back(code, ws_loop);
    emit_patch_jump(code, not_tab);
    emit_patch_jump(code, ws_done);

    // ---- optional sign ----
    emit_mov_reg_imm64(code, REG_RBX, 0); // sign flag (tag no longer needed)
    emit_cmp_reg_reg(code, REG_RSI, REG_RDI);
    int no_sign_check = emit_jcc_rel32(code, COND_GE);
    emit_load_byte_reg(code, REG_RCX, REG_RSI);
    emit_cmp_reg_imm32(code, REG_RCX, '-');
    int not_minus = emit_jcc_rel32(code, COND_NE);
    emit_mov_reg_imm64(code, REG_RBX, 1);
    emit_add_reg_imm8(code, REG_RSI, 1);
    int sign_done = emit_jmp_rel32(code);
    emit_patch_jump(code, not_minus);
    emit_cmp_reg_imm32(code, REG_RCX, '+');
    int not_plus = emit_jcc_rel32(code, COND_NE);
    emit_add_reg_imm8(code, REG_RSI, 1);
    emit_patch_jump(code, not_plus);
    emit_patch_jump(code, sign_done);
    emit_patch_jump(code, no_sign_check);

    emit_pxor_xmm_xmm(code, XMM0); // accumulator = 0.0

    // ---- integer digits ----
    int int_loop = code->count;
    emit_cmp_reg_reg(code, REG_RSI, REG_RDI);
    int int_done = emit_jcc_rel32(code, COND_GE);
    emit_load_byte_reg(code, REG_RCX, REG_RSI);
    emit_cmp_reg_imm32(code, REG_RCX, '0');
    int int_lt = emit_jcc_rel32(code, COND_LT);
    emit_cmp_reg_imm32(code, REG_RCX, '9');
    int int_gt = emit_jcc_rel32(code, COND_GT);

    load_double_const(XMM1, 10.0);
    emit_mulsd(code, XMM0, XMM1);
    emit_sub_reg_imm8(code, REG_RCX, '0');
    emit_cvtsi2sd(code, XMM1, REG_RCX);
    emit_addsd(code, XMM0, XMM1);
    emit_add_reg_imm8(code, REG_RSI, 1);
    emit_jmp_back(code, int_loop);

    emit_patch_jump(code, int_lt);
    emit_patch_jump(code, int_gt);
    emit_patch_jump(code, int_done);

    // ---- optional fraction ----
    emit_cmp_reg_reg(code, REG_RSI, REG_RDI);
    int no_frac_check = emit_jcc_rel32(code, COND_GE);
    emit_load_byte_reg(code, REG_RCX, REG_RSI);
    emit_cmp_reg_imm32(code, REG_RCX, '.');
    int not_dot = emit_jcc_rel32(code, COND_NE);
    emit_add_reg_imm8(code, REG_RSI, 1);

    load_double_const(XMM2, 0.1); // place value, halves each digit
    int frac_loop = code->count;
    emit_cmp_reg_reg(code, REG_RSI, REG_RDI);
    int frac_done = emit_jcc_rel32(code, COND_GE);
    emit_load_byte_reg(code, REG_RCX, REG_RSI);
    emit_cmp_reg_imm32(code, REG_RCX, '0');
    int frac_lt = emit_jcc_rel32(code, COND_LT);
    emit_cmp_reg_imm32(code, REG_RCX, '9');
    int frac_gt = emit_jcc_rel32(code, COND_GT);

    emit_sub_reg_imm8(code, REG_RCX, '0');
    emit_cvtsi2sd(code, XMM1, REG_RCX);
    emit_mulsd(code, XMM1, XMM2); // digit * place
    emit_addsd(code, XMM0, XMM1);
    load_double_const(XMM1, 0.1);
    emit_mulsd(code, XMM2, XMM1); // place *= 0.1
    emit_add_reg_imm8(code, REG_RSI, 1);
    emit_jmp_back(code, frac_loop);

    emit_patch_jump(code, frac_lt);
    emit_patch_jump(code, frac_gt);
    emit_patch_jump(code, frac_done);

    emit_patch_jump(code, not_dot);
    emit_patch_jump(code, no_frac_check);

    // ---- apply sign ----
    emit_cmp_reg_imm32(code, REG_RBX, 0);
    int no_negate = emit_jcc_rel32(code, COND_E);
    emit_pxor_xmm_xmm(code, XMM1);
    emit_subsd(code, XMM1, XMM0);
    emit_movsd_xmm_xmm(code, XMM0, XMM1);
    emit_patch_jump(code, no_negate);

    emit_movq_reg_from_xmm(code, REG_RAX, XMM0);
    emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}
