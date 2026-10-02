#include <stdint.h>
#include "codegen/file_path.h"
#include "codegen/bytes.h"
#include "codegen/heap.h"
#include "parser/vars.h"

void filepath_emit_nullterm(CodeBuf *code) {
    int32_t base = higher_order_scratch_offset();
    emit_store_mem_disp32(code, REG_RBP, base + 0, REG_RSI); // src
    emit_store_mem_disp32(code, REG_RBP, base + 8, REG_RDX); // length

    emit_mov_reg_reg(code, REG_RDI, REG_RDX);
    emit_add_reg_imm8(code, REG_RDI, 1); // +1 for the null terminator
    heap_emit_alloc(code); // RAX = buf
    emit_store_mem_disp32(code, REG_RBP, base + 16, REG_RAX);

    emit_mov_reg_reg(code, REG_RDI, REG_RAX);
    emit_load_mem_disp32(code, REG_RBX, REG_RBP, base + 0); // src
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 8); // length
    bytes_emit_copy(code);

    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 16); // buf
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 8);  // length
    emit_mov_reg_reg(code, REG_RCX, REG_RAX);
    emit_add_reg_reg(code, REG_RCX, REG_RDX);
    emit_store_byte_imm(code, REG_RCX, 0); // null terminator
    // RAX already holds buf, the return value
}
