#include "codegen/H/platform/linux_call.h"
#include "codegen/H/emit/emit_internal.h"
#include "parser/H/declarations/vars.h"

// AND RSP, imm8 (sign-extended) -- opcode 0x83 /4. Same encoding
// win_call.c's own emit_and_rsp_imm8 uses; kept as a separate local
// copy rather than exporting that one, matching this project's existing
// pattern of small leaf instruction-encoders staying local to whichever
// file needs them (win_call.c has several of its own the same way).
static void emit_and_rsp_imm8(CodeBuf *code, int8_t imm) {
    emit_byte(code, REX_W);
    emit_byte(code, 0x83);
    emit_byte(code, modrm_reg((Reg)4, REG_RSP)); // /4 = AND
    emit_byte(code, (uint8_t)imm);
}

void linux_abi_call_begin(CodeBuf *code) {
    emit_store_mem_disp32(code, REG_RBP, win_call_scratch_offset(), REG_RSP);
    emit_and_rsp_imm8(code, -16);
}

void linux_abi_call_end(CodeBuf *code) {
    emit_load_mem_disp32(code, REG_RSP, REG_RBP, win_call_scratch_offset());
}
