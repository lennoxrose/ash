#include "codegen/string_builtins.h"
#include "codegen/string_alloc.h"
#include "codegen/bytes.h"
#include "codegen/emit.h"
#include "codegen/value.h"
#include "parser/parser.h"

// Advances RSI forward past leading whitespace, then RDI backward past
// trailing whitespace (RDI starts as the exclusive end pointer), each via
// a small 4-way character check ( ' ' '\t' '\n' '\r', matching ashvm's
// own set) inlined rather than a shared byte-classifier helper -- it's
// only used twice, in a shape specific to which direction it's scanning.
void codegen_builtin_trim(void) {
    emit_pop_reg(code, REG_RAX); // src payload
    emit_pop_reg(code, REG_RBX); // tag (ignored)
    emit_load_mem_disp32(code, REG_RDX, REG_RAX, -8); // length
    emit_mov_reg_reg(code, REG_RSI, REG_RAX); // start cursor
    emit_mov_reg_reg(code, REG_RDI, REG_RAX);
    emit_add_reg_reg(code, REG_RDI, REG_RDX); // end cursor (exclusive)

    int trim_start = code->count;
    emit_cmp_reg_reg(code, REG_RSI, REG_RDI);
    int start_done = emit_jcc_rel32(code, COND_GE);
    emit_load_byte_reg(code, REG_RCX, REG_RSI);
    emit_cmp_reg_imm32(code, REG_RCX, ' ');
    int s1 = emit_jcc_rel32(code, COND_E);
    emit_cmp_reg_imm32(code, REG_RCX, '\t');
    int s2 = emit_jcc_rel32(code, COND_E);
    emit_cmp_reg_imm32(code, REG_RCX, '\n');
    int s3 = emit_jcc_rel32(code, COND_E);
    emit_cmp_reg_imm32(code, REG_RCX, '\r');
    int s4 = emit_jcc_rel32(code, COND_E);
    int not_ws_start = emit_jmp_rel32(code);
    emit_patch_jump(code, s1); emit_patch_jump(code, s2);
    emit_patch_jump(code, s3); emit_patch_jump(code, s4);
    emit_add_reg_imm8(code, REG_RSI, 1);
    emit_jmp_back(code, trim_start);
    emit_patch_jump(code, not_ws_start);
    emit_patch_jump(code, start_done);

    int trim_end = code->count;
    emit_cmp_reg_reg(code, REG_RDI, REG_RSI);
    int end_done = emit_jcc_rel32(code, COND_LE);
    emit_mov_reg_reg(code, REG_RAX, REG_RDI);
    emit_sub_reg_imm8(code, REG_RAX, 1);
    emit_load_byte_reg(code, REG_RCX, REG_RAX);
    emit_cmp_reg_imm32(code, REG_RCX, ' ');
    int e1 = emit_jcc_rel32(code, COND_E);
    emit_cmp_reg_imm32(code, REG_RCX, '\t');
    int e2 = emit_jcc_rel32(code, COND_E);
    emit_cmp_reg_imm32(code, REG_RCX, '\n');
    int e3 = emit_jcc_rel32(code, COND_E);
    emit_cmp_reg_imm32(code, REG_RCX, '\r');
    int e4 = emit_jcc_rel32(code, COND_E);
    int not_ws_end = emit_jmp_rel32(code);
    emit_patch_jump(code, e1); emit_patch_jump(code, e2);
    emit_patch_jump(code, e3); emit_patch_jump(code, e4);
    emit_sub_reg_imm8(code, REG_RDI, 1);
    emit_jmp_back(code, trim_end);
    emit_patch_jump(code, not_ws_end);
    emit_patch_jump(code, end_done);

    emit_mov_reg_reg(code, REG_RDX, REG_RDI);
    emit_sub_reg_reg(code, REG_RDX, REG_RSI); // length
    emit_mov_reg_reg(code, REG_RBX, REG_RSI); // stash trimmed start (survives the alloc call)

    string_alloc_emit_prefixed(code); // RAX = new block, RDX = length (preserved)

    emit_mov_reg_reg(code, REG_RSI, REG_RAX); // stash block (bytes_emit_copy preserves RSI)
    emit_mov_reg_reg(code, REG_RDI, REG_RSI);
    emit_add_reg_imm8(code, REG_RDI, 8); // dest
    bytes_emit_copy(code); // RBX=src (already set), RDX=length (already set)

    emit_mov_reg_reg(code, REG_RAX, REG_RSI);
    emit_add_reg_imm8(code, REG_RAX, 8); // payload
    emit_mov_reg_imm64(code, REG_RBX, TAG_STRING);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}
