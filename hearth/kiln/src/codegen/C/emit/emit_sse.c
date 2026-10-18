#include "codegen/H/emit/emit_sse.h"
#include "codegen/H/emit/emit_internal.h"

// ModRM for two xmm operands (or GP<->xmm), register-direct -- same
// mod=11 shape modrm_reg() builds, xmm register numbers 0-7 encode
// identically to GP register numbers 0-7 in ModRM.
static uint8_t modrm_xmm(int reg_field, int rm_field) {
    return (uint8_t)(0xC0 | ((reg_field & 7) << 3) | (rm_field & 7));
}

void emit_movq_xmm_from_reg(CodeBuf *buf, XReg dst, Reg src) {
    emit_byte(buf, 0x66);
    emit_byte(buf, REX_W);
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x6E); // MOVQ xmm, r/m64
    emit_byte(buf, modrm_xmm(dst, src));
}

void emit_movq_reg_from_xmm(CodeBuf *buf, Reg dst, XReg src) {
    emit_byte(buf, 0x66);
    emit_byte(buf, REX_W);
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x7E); // MOVQ r/m64, xmm
    emit_byte(buf, modrm_xmm(src, dst));
}

void emit_movsd_xmm_xmm(CodeBuf *buf, XReg dst, XReg src) {
    emit_byte(buf, 0xF2);
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x10); // MOVSD xmm1, xmm2
    emit_byte(buf, modrm_xmm(dst, src));
}

void emit_movsd_load_disp32(CodeBuf *buf, XReg dst, Reg base, int32_t disp) {
    emit_byte(buf, 0xF2);
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x10); // MOVSD xmm, [base+disp32]
    emit_byte(buf, modrm_mem_disp32((Reg)dst, base));
    emit_i32(buf, disp);
}

void emit_movsd_store_disp32(CodeBuf *buf, Reg base, int32_t disp, XReg src) {
    emit_byte(buf, 0xF2);
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x11); // MOVSD [base+disp32], xmm
    emit_byte(buf, modrm_mem_disp32((Reg)src, base));
    emit_i32(buf, disp);
}

void emit_pxor_xmm_xmm(CodeBuf *buf, XReg reg) {
    emit_byte(buf, 0x66);
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0xEF); // PXOR xmm, xmm
    emit_byte(buf, modrm_xmm(reg, reg));
}

void emit_addsd(CodeBuf *buf, XReg dst, XReg src) {
    emit_byte(buf, 0xF2); emit_byte(buf, 0x0F); emit_byte(buf, 0x58);
    emit_byte(buf, modrm_xmm(dst, src));
}

void emit_subsd(CodeBuf *buf, XReg dst, XReg src) {
    emit_byte(buf, 0xF2); emit_byte(buf, 0x0F); emit_byte(buf, 0x5C);
    emit_byte(buf, modrm_xmm(dst, src));
}

void emit_mulsd(CodeBuf *buf, XReg dst, XReg src) {
    emit_byte(buf, 0xF2); emit_byte(buf, 0x0F); emit_byte(buf, 0x59);
    emit_byte(buf, modrm_xmm(dst, src));
}

void emit_divsd(CodeBuf *buf, XReg dst, XReg src) {
    emit_byte(buf, 0xF2); emit_byte(buf, 0x0F); emit_byte(buf, 0x5E);
    emit_byte(buf, modrm_xmm(dst, src));
}

void emit_sqrtsd(CodeBuf *buf, XReg dst, XReg src) {
    emit_byte(buf, 0xF2); emit_byte(buf, 0x0F); emit_byte(buf, 0x51);
    emit_byte(buf, modrm_xmm(dst, src));
}

void emit_andpd(CodeBuf *buf, XReg dst, XReg src) {
    emit_byte(buf, 0x66); emit_byte(buf, 0x0F); emit_byte(buf, 0x54);
    emit_byte(buf, modrm_xmm(dst, src));
}

void emit_cvttsd2si(CodeBuf *buf, Reg dst, XReg src) {
    emit_byte(buf, 0xF2);
    emit_byte(buf, REX_W);
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x2C); // CVTTSD2SI r64, xmm
    emit_byte(buf, modrm_xmm(dst, src));
}

void emit_cvtsi2sd(CodeBuf *buf, XReg dst, Reg src) {
    emit_byte(buf, 0xF2);
    emit_byte(buf, REX_W);
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x2A); // CVTSI2SD xmm, r/m64
    emit_byte(buf, modrm_xmm(dst, src));
}

void emit_ucomisd(CodeBuf *buf, XReg a, XReg b) {
    emit_byte(buf, 0x66);
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x2E); // UCOMISD xmm1, xmm2
    emit_byte(buf, modrm_xmm(a, b));
}

void emit_push_xmm(CodeBuf *buf, XReg src) {
    emit_sub_reg_imm8(buf, REG_RSP, 8);
    emit_byte(buf, 0xF2);
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x11); // MOVSD [rsp], xmm
    emit_byte(buf, (uint8_t)(((src & 7) << 3) | 0x04)); // mod=00, rm=100 (SIB follows)
    emit_byte(buf, 0x24); // SIB: scale=00, index=100(none), base=100(rsp)
}

void emit_pop_xmm(CodeBuf *buf, XReg dst) {
    emit_byte(buf, 0xF2);
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x10); // MOVSD xmm, [rsp]
    emit_byte(buf, (uint8_t)(((dst & 7) << 3) | 0x04));
    emit_byte(buf, 0x24);
    emit_add_reg_imm8(buf, REG_RSP, 8);
}
