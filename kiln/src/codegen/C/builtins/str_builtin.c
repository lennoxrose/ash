#include <string.h>
#include "codegen/H/builtins/convert_builtins.h"
#include "codegen/H/emit/emit.h"
#include "codegen/H/emit/emit_sse.h"
#include "codegen/H/emit/value.h"
#include "codegen/H/runtime/heap.h"
#include "codegen/H/runtime/errors.h"
#include "codegen/H/strings/strings.h"
#include "codegen/H/runtime/number_format.h"
#include "codegen/H/emit/bytes.h"
#include "parser/H/core/parser.h"

// str(number): formats via number_format_emit (see its header for the format)
// into a stack scratch region, then copies the text into a fresh string.
void codegen_builtin_str(void) {
    emit_pop_reg(code, REG_RAX); // payload
    emit_pop_reg(code, REG_RBX); // tag
    emit_cmp_reg_imm32(code, REG_RBX, TAG_NUMBER);
    int is_number = emit_jcc_rel32(code, COND_E);
    emit_cmp_reg_imm32(code, REG_RBX, TAG_BOOL);
    int is_bool = emit_jcc_rel32(code, COND_E);
    errors_emit_die(code, "runtime error: str() expects a number");
    emit_patch_jump(code, is_bool);
    emit_cmp_reg_imm32(code, REG_RAX, 0); // payload bits: 0.0 is all zero
    int bool_no = emit_jcc_rel32(code, COND_E);
    codegen_string_literal_bytes("yes", 3);
    int bool_done_yes = emit_jmp_rel32(code);
    emit_patch_jump(code, bool_no);
    codegen_string_literal_bytes("no", 2);
    int bool_done = emit_jmp_rel32(code);
    emit_patch_jump(code, is_number);
    number_format_emit(code); // RCX = text start, RDX = text end (on the stack)
    emit_sub_reg_reg(code, REG_RDX, REG_RCX); // RDX = content length

    // ---- heap-allocate a length-prefixed block and copy the digits in ----
    emit_mov_reg_reg(code, REG_RDI, REG_RDX);
    emit_add_reg_imm8(code, REG_RDI, 8);
    heap_emit_alloc(code); // RAX = new block, clobbers RSI

    emit_store_mem_disp32(code, REG_RAX, 0, REG_RDX); // length prefix
    emit_mov_reg_reg(code, REG_RSI, REG_RAX);          // stash block addr (bytes_emit_copy preserves RSI)
    emit_mov_reg_reg(code, REG_RDI, REG_RSI);
    emit_add_reg_imm8(code, REG_RDI, 8);               // dest = block+8
    emit_mov_reg_reg(code, REG_RBX, REG_RCX);          // src = digit buffer start
    bytes_emit_copy(code); // clobbers RAX,RBX,RCX,RDX,RDI; leaves RSI = block addr

    emit_add_reg_imm8(code, REG_RSP, NUMBER_FORMAT_SCRATCH); // release the digit scratch

    emit_mov_reg_reg(code, REG_RAX, REG_RSI);
    emit_add_reg_imm8(code, REG_RAX, 8); // payload = block+8

    emit_mov_reg_imm64(code, REG_RBX, TAG_STRING);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
    emit_patch_jump(code, bool_done_yes);
    emit_patch_jump(code, bool_done);
}
