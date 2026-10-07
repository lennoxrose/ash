#include <string.h>
#include "codegen/emit_internal.h"

void emit_store_mem_disp32(CodeBuf *buf, Reg base, int32_t disp, Reg src) {
    emit_byte(buf, REX_W);
    emit_byte(buf, 0x89); // MOV r/m64, r64
    emit_byte(buf, modrm_mem_disp32(src, base));
    emit_i32(buf, disp);
}

void emit_load_mem_disp32(CodeBuf *buf, Reg dst, Reg base, int32_t disp) {
    emit_byte(buf, REX_W);
    emit_byte(buf, 0x8B); // MOV r64, r/m64
    emit_byte(buf, modrm_mem_disp32(dst, base));
    emit_i32(buf, disp);
}

void emit_push_reg(CodeBuf *buf, Reg reg) {
    emit_byte(buf, (uint8_t)(0x50 + reg));
}

void emit_pop_reg(CodeBuf *buf, Reg reg) {
    emit_byte(buf, (uint8_t)(0x58 + reg));
}

void emit_store_byte_imm(CodeBuf *buf, Reg base, uint8_t imm) {
    emit_byte(buf, 0xC6); // MOV r/m8, imm8
    emit_byte(buf, modrm_mem((Reg)0, base));
    emit_byte(buf, imm);
}

void emit_store_byte_reg(CodeBuf *buf, Reg base, Reg src) {
    emit_byte(buf, 0x88); // MOV r/m8, r8 (src must be AL/CL/DL/BL -- no REX needed)
    emit_byte(buf, modrm_mem(src, base));
}

void emit_load_byte_reg(CodeBuf *buf, Reg dst, Reg base) {
    emit_byte(buf, REX_W);
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0xB6); // MOVZX r64, r/m8
    emit_byte(buf, modrm_mem(dst, base));
}

void emit_syscall(CodeBuf *buf) {
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x05);
}

int emit_jcc_rel32(CodeBuf *buf, Cond cc) {
    emit_byte(buf, 0x0F);
    emit_byte(buf, (uint8_t)(0x80 | cc));
    int patch_offset = buf->count;
    emit_i32(buf, 0); // placeholder
    return patch_offset;
}

int emit_jmp_rel32(CodeBuf *buf) {
    emit_byte(buf, 0xE9);
    int patch_offset = buf->count;
    emit_i32(buf, 0); // placeholder
    return patch_offset;
}

void emit_patch_jump(CodeBuf *buf, int patch_offset) {
    int32_t rel = buf->count - (patch_offset + 4);
    memcpy(buf->code + patch_offset, &rel, 4);
}

void emit_jmp_back(CodeBuf *buf, int target_offset) {
    emit_byte(buf, 0xE9);
    int32_t rel = target_offset - (buf->count + 4);
    emit_i32(buf, rel);
}

void emit_jcc_back(CodeBuf *buf, Cond cc, int target_offset) {
    emit_byte(buf, 0x0F);
    emit_byte(buf, (uint8_t)(0x80 | cc));
    int32_t rel = target_offset - (buf->count + 4);
    emit_i32(buf, rel);
}

void emit_call_back(CodeBuf *buf, int target_offset) {
    emit_byte(buf, 0xE8);
    int32_t rel = target_offset - (buf->count + 4);
    emit_i32(buf, rel);
}

void emit_ret(CodeBuf *buf) {
    emit_byte(buf, 0xC3);
}

void emit_call_indirect(CodeBuf *buf, Reg target) {
    emit_byte(buf, 0xFF);
    emit_byte(buf, modrm_reg((Reg)2, target)); // /2 = CALL r/m64
}

void emit_jmp_indirect(CodeBuf *buf, Reg target) {
    emit_byte(buf, 0xFF);
    emit_byte(buf, modrm_reg((Reg)4, target)); // /4 = JMP r/m64
}

int emit_lea_rip_disp32(CodeBuf *buf, Reg dst) {
    emit_byte(buf, REX_W);
    emit_byte(buf, 0x8D); // LEA r64, m
    emit_byte(buf, (uint8_t)(((dst & 7) << 3) | 5)); // mod=00, rm=101 -> RIP-relative
    int patch_offset = buf->count;
    emit_i32(buf, 0); // placeholder
    return patch_offset;
}

// Same instruction, for the already-emitted-earlier case (e.g. embedded
// data emitted before the code that references it, jumped over) -- same
// "compute the offset immediately" shape as emit_jmp_back/emit_call_back.
void emit_lea_rip_back(CodeBuf *buf, Reg dst, int target_offset) {
    emit_byte(buf, REX_W);
    emit_byte(buf, 0x8D);
    emit_byte(buf, (uint8_t)(((dst & 7) << 3) | 5));
    int32_t rel = target_offset - (buf->count + 4);
    emit_i32(buf, rel);
}

// CALL [disp32] -- x86-64's 32-bit-absolute-displacement addressing mode
// (mod=00, rm=100 selects SIB, SIB base=101 with no index/base register
// means "just disp32", distinct from mod=00,rm=101's RIP-relative
// meaning). Dereferences the given absolute address and calls through
// the result, touching NO general-purpose register at all -- needed for
// Plan B's GOT calls specifically because kiln's runtime routines each
// have their own fixed argument registers (RAX/RBX/RDI/RDX depending on
// which routine), so there's no register universally safe to clobber
// just to hold the GOT address first. Only valid for addresses that fit
// in 32 bits (sign-extended) -- true for every address kiln's fixed,
// low, non-PIE load base ever produces.
void emit_call_abs32(CodeBuf *buf, uint32_t addr) {
    emit_byte(buf, 0xFF);
    emit_byte(buf, 0x14); // ModRM: mod=00, reg=010(/2=CALL), rm=100(SIB)
    emit_byte(buf, 0x25); // SIB: scale=00, index=100(none), base=101(disp32 only)
    emit_i32(buf, (int32_t)addr);
}
