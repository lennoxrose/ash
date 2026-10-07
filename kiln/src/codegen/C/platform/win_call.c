#include "codegen/H/platform/win_call.h"
#include "codegen/H/emit/emit_internal.h"
#include "codegen/H/emit/layout.h"
#include "parser/H/declarations/vars.h"

// AND RSP, imm8 (sign-extended) -- opcode 0x83 /4. Nothing else in kiln
// needs a register-immediate AND, so this stays local instead of joining
// emit.c's public primitives.
static void emit_and_rsp_imm8(CodeBuf *code, int8_t imm) {
    emit_byte(code, REX_W);
    emit_byte(code, 0x83);
    emit_byte(code, modrm_reg((Reg)4, REG_RSP)); // /4 = AND
    emit_byte(code, (uint8_t)imm);
}

// R8/R9 need REX.B to extend the 3-bit register field in both the
// "+reg" immediate-move opcode and a ModRM rm field -- kiln's Reg enum
// (emit.h) only covers RAX-RDI (0-7), so these stay as tiny local helpers
// parameterized by the raw register number (8 or 9) rather than growing
// the public enum for a feature only Windows argument-passing needs.
static void emit_mov_r8_9_imm64(CodeBuf *code, int reg89, uint64_t imm) {
    emit_byte(code, 0x49); // REX.WB
    emit_byte(code, (uint8_t)(0xB8 + (reg89 - 8)));
    emit_u64(code, imm);
}

static void emit_mov_r8_9_reg(CodeBuf *code, int reg89, Reg src) {
    emit_byte(code, 0x49); // REX.WB
    emit_byte(code, 0x89); // MOV r/m64, r64
    emit_byte(code, modrm_reg(src, (Reg)(reg89 - 8)));
}

// MOV [rsp+disp32], src -- RSP as a memory base always needs a SIB byte
// (index=100 "none", base=100=RSP, scale=00), unlike RBP which
// emit_mem.c's modrm_mem_disp32 already handles directly. Nothing else in
// kiln addresses [rsp+disp] (existing code routes through RBP-relative
// scratch instead), so this is local to the one place that genuinely
// needs it: Windows stack arguments (5th+), which the ABI requires to
// physically live above the shadow space at call time.
static void emit_store_rsp_disp32(CodeBuf *code, int32_t disp, Reg src) {
    emit_byte(code, REX_W);
    emit_byte(code, 0x89);
    emit_byte(code, (uint8_t)(0x80 | ((src & 7) << 3) | 4));
    emit_byte(code, 0x24); // SIB: scale=00, index=100 (none), base=100 (RSP)
    emit_i32(code, disp);
}

void win_call_begin(CodeBuf *code, int stack_arg_count) {
    emit_store_mem_disp32(code, REG_RBP, win_call_scratch_offset(), REG_RSP);
    emit_and_rsp_imm8(code, -16);
    int32_t needed = 32 + 8 * stack_arg_count;
    int32_t reserved = (needed + 15) / 16 * 16;
    emit_sub_reg_imm8(code, REG_RSP, (int8_t)reserved);
}

void win_call_arg_reg(CodeBuf *code, int index, Reg src) {
    switch (index) {
        case 0: emit_mov_reg_reg(code, REG_RCX, src); break;
        case 1: emit_mov_reg_reg(code, REG_RDX, src); break;
        case 2: emit_mov_r8_9_reg(code, 8, src); break;
        case 3: emit_mov_r8_9_reg(code, 9, src); break;
    }
}

void win_call_arg_imm64(CodeBuf *code, int index, uint64_t imm) {
    switch (index) {
        case 0: emit_mov_reg_imm64(code, REG_RCX, imm); break;
        case 1: emit_mov_reg_imm64(code, REG_RDX, imm); break;
        case 2: emit_mov_r8_9_imm64(code, 8, imm); break;
        case 3: emit_mov_r8_9_imm64(code, 9, imm); break;
    }
}

void win_call_stack_arg_reg(CodeBuf *code, int stack_index, Reg src) {
    emit_store_rsp_disp32(code, 32 + 8 * stack_index, src);
}

void win_call_stack_arg_imm64(CodeBuf *code, int stack_index, uint64_t imm) {
    emit_mov_reg_imm64(code, REG_RAX, imm);
    emit_store_rsp_disp32(code, 32 + 8 * stack_index, REG_RAX);
}

void win_call_import(CodeBuf *code, PeImport which) {
    emit_mov_reg_imm64(code, REG_RAX, pe_import_addr(which));
    emit_load_mem_disp32(code, REG_RAX, REG_RAX, 0); // resolved function pointer
    emit_call_indirect(code, REG_RAX);
}

void win_call_end(CodeBuf *code) {
    emit_load_mem_disp32(code, REG_RSP, REG_RBP, win_call_scratch_offset());
}

// CALL [rip+disp32] -- unlike win_call_import's kernel32.dll calls (real
// Windows API, needs the full shadow-space dance above), libkilnrt.dll's
// exports are kiln's OWN code using kiln's OWN simple per-routine
// register conventions (RAX/RBX/RDI/RDX depending on which routine, see
// codegen/C/runtime/heap.c etc.) -- exactly like the Linux GOT call
// (elf/C/elf_dynamic_call.c's emit_call_abs32), no register is safe to
// clobber loading the IAT address first. Can't reuse emit_call_abs32's
// technique directly, though: kiln's PE image base (0x140000000) is
// already past the 32-bit absolute-displacement addressing mode's
// range, but RIP-relative reaches it fine since the IAT slot and the
// calling code both live in the same small, nearby image.
void win_call_runtime_import(CodeBuf *code, RuntimeImport which) {
    uint64_t target = pe_runtime_import_addr(which);
    emit_byte(code, 0xFF);
    emit_byte(code, 0x15); // ModRM: mod=00, reg=010(/2=CALL), rm=101(RIP-relative)
    uint64_t next_instr_addr = kiln_code_base() + (uint64_t)(code->count + 4);
    int32_t rel = (int32_t)((int64_t)target - (int64_t)next_instr_addr);
    emit_i32(code, rel);
}
