#include <stdlib.h>
#include <string.h>
#include "codegen/emit_internal.h"

void code_init(CodeBuf *buf) {
    buf->code = NULL;
    buf->count = 0;
    buf->capacity = 0;
}

void emit_byte(CodeBuf *buf, uint8_t b) {
    if (buf->count >= buf->capacity) {
        buf->capacity = buf->capacity == 0 ? 256 : buf->capacity * 2;
        buf->code = realloc(buf->code, buf->capacity);
    }
    buf->code[buf->count++] = b;
}

void emit_i32(CodeBuf *buf, int32_t v) {
    uint8_t b[4];
    memcpy(b, &v, 4);
    for (int i = 0; i < 4; i++) emit_byte(buf, b[i]);
}

void emit_u64(CodeBuf *buf, uint64_t v) {
    uint8_t b[8];
    memcpy(b, &v, 8);
    for (int i = 0; i < 8; i++) emit_byte(buf, b[i]);
}

// ModRM byte for two register operands, register-direct addressing mode.
uint8_t modrm_reg(Reg reg_field, Reg rm_field) {
    return (uint8_t)(0xC0 | ((reg_field & 7) << 3) | (rm_field & 7));
}

// ModRM byte for a register-indirect memory operand [base], mod=00 -- only
// valid when base isn't RSP (needs a SIB byte) or RBP (mod=00,rm=101 means
// RIP-relative instead), which is why this module's callers are required
// to route byte-buffer pointers through a plain register like RCX.
uint8_t modrm_mem(Reg reg_field, Reg base) {
    return (uint8_t)(((reg_field & 7) << 3) | (base & 7));
}

// ModRM byte for a register-indirect memory operand with a 32-bit
// displacement [base+disp32], mod=10 -- safe for RBP (mod=10 has no
// special-case meaning, unlike mod=00). RSP as base still needs a SIB
// byte even at mod=10, so this is only used with RBP in practice.
uint8_t modrm_mem_disp32(Reg reg_field, Reg base) {
    return (uint8_t)(0x80 | ((reg_field & 7) << 3) | (base & 7));
}

void emit_mov_reg_imm64(CodeBuf *buf, Reg dst, uint64_t imm) {
    emit_byte(buf, REX_W);
    emit_byte(buf, (uint8_t)(0xB8 + dst));
    emit_u64(buf, imm);
}

int emit_mov_reg_imm64_patchable(CodeBuf *buf, Reg dst) {
    emit_byte(buf, REX_W);
    emit_byte(buf, (uint8_t)(0xB8 + dst));
    int patch_offset = buf->count;
    emit_u64(buf, 0); // placeholder
    return patch_offset;
}

void emit_patch_imm64(CodeBuf *buf, int patch_offset, uint64_t value) {
    memcpy(buf->code + patch_offset, &value, 8);
}

void emit_mov_reg_reg(CodeBuf *buf, Reg dst, Reg src) {
    emit_byte(buf, REX_W);
    emit_byte(buf, 0x89); // MOV r/m64, r64
    emit_byte(buf, modrm_reg(src, dst));
}

void emit_add_reg_reg(CodeBuf *buf, Reg dst, Reg src) {
    emit_byte(buf, REX_W);
    emit_byte(buf, 0x01); // ADD r/m64, r64
    emit_byte(buf, modrm_reg(src, dst));
}

void emit_add_reg_imm8(CodeBuf *buf, Reg reg, int8_t imm) {
    emit_byte(buf, REX_W);
    emit_byte(buf, 0x83); // ADD/SUB r/m64, imm8 group
    emit_byte(buf, modrm_reg((Reg)0, reg)); // /0 = ADD
    emit_byte(buf, (uint8_t)imm);
}

void emit_add_reg_imm32(CodeBuf *buf, Reg reg, int32_t imm) {
    emit_byte(buf, REX_W);
    emit_byte(buf, 0x81);
    emit_byte(buf, modrm_reg((Reg)0, reg)); // /0 = ADD
    emit_i32(buf, imm);
}

void emit_sub_reg_reg(CodeBuf *buf, Reg dst, Reg src) {
    emit_byte(buf, REX_W);
    emit_byte(buf, 0x29); // SUB r/m64, r64
    emit_byte(buf, modrm_reg(src, dst));
}

void emit_sub_reg_imm8(CodeBuf *buf, Reg reg, int8_t imm) {
    emit_byte(buf, REX_W);
    emit_byte(buf, 0x83);
    emit_byte(buf, modrm_reg((Reg)5, reg)); // /5 = SUB
    emit_byte(buf, (uint8_t)imm);
}

void emit_sub_reg_imm32(CodeBuf *buf, Reg reg, int32_t imm) {
    emit_byte(buf, REX_W);
    emit_byte(buf, 0x81);
    emit_byte(buf, modrm_reg((Reg)5, reg)); // /5 = SUB
    emit_i32(buf, imm);
}

void emit_imul_reg_reg(CodeBuf *buf, Reg dst, Reg src) {
    emit_byte(buf, REX_W);
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0xAF); // IMUL r64, r/m64
    emit_byte(buf, modrm_reg(dst, src));
}

void emit_idiv_reg(CodeBuf *buf, Reg reg) {
    emit_byte(buf, REX_W);
    emit_byte(buf, 0xF7);
    emit_byte(buf, modrm_reg((Reg)7, reg)); // /7 = IDIV
}

void emit_neg_reg(CodeBuf *buf, Reg reg) {
    emit_byte(buf, REX_W);
    emit_byte(buf, 0xF7);
    emit_byte(buf, modrm_reg((Reg)3, reg)); // /3 = NEG
}

void emit_dec_reg(CodeBuf *buf, Reg reg) {
    emit_byte(buf, REX_W);
    emit_byte(buf, 0xFF);
    emit_byte(buf, modrm_reg((Reg)1, reg)); // /1 = DEC
}

void emit_cqo(CodeBuf *buf) {
    emit_byte(buf, REX_W);
    emit_byte(buf, 0x99);
}

void emit_cmp_reg_imm32(CodeBuf *buf, Reg reg, int32_t imm) {
    emit_byte(buf, REX_W);
    emit_byte(buf, 0x81);
    emit_byte(buf, modrm_reg((Reg)7, reg)); // /7 = CMP
    emit_i32(buf, imm);
}

void emit_cmp_reg_reg(CodeBuf *buf, Reg a, Reg b) {
    emit_byte(buf, REX_W);
    emit_byte(buf, 0x39); // CMP r/m64, r64  (computes a - b, sets flags)
    emit_byte(buf, modrm_reg(b, a));
}

void emit_and_reg_reg(CodeBuf *buf, Reg dst, Reg src) {
    emit_byte(buf, REX_W);
    emit_byte(buf, 0x21); // AND r/m64, r64
    emit_byte(buf, modrm_reg(src, dst));
}

void emit_or_reg_reg(CodeBuf *buf, Reg dst, Reg src) {
    emit_byte(buf, REX_W);
    emit_byte(buf, 0x09); // OR r/m64, r64
    emit_byte(buf, modrm_reg(src, dst));
}

void emit_xor_reg_reg(CodeBuf *buf, Reg dst, Reg src) {
    emit_byte(buf, REX_W);
    emit_byte(buf, 0x31); // XOR r/m64, r64
    emit_byte(buf, modrm_reg(src, dst));
}

void emit_not_reg(CodeBuf *buf, Reg reg) {
    emit_byte(buf, REX_W);
    emit_byte(buf, 0xF7);
    emit_byte(buf, modrm_reg((Reg)2, reg)); // /2 = NOT
}

void emit_shl_reg_cl(CodeBuf *buf, Reg reg) {
    emit_byte(buf, REX_W);
    emit_byte(buf, 0xD3);
    emit_byte(buf, modrm_reg((Reg)4, reg)); // /4 = SHL r/m64, CL
}

void emit_sar_reg_cl(CodeBuf *buf, Reg reg) {
    emit_byte(buf, REX_W);
    emit_byte(buf, 0xD3);
    emit_byte(buf, modrm_reg((Reg)7, reg)); // /7 = SAR r/m64, CL
}

void emit_sar_reg_by1(CodeBuf *buf, Reg reg) {
    emit_byte(buf, REX_W);
    emit_byte(buf, 0xD1);
    emit_byte(buf, modrm_reg((Reg)7, reg)); // /7 = SAR r/m64, 1
}
