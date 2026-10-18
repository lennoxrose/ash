#include <stdint.h>
#include "codegen/H/strings/string_builtins.h"
#include "codegen/H/strings/string_alloc.h"
#include "codegen/H/emit/emit.h"
#include "codegen/H/emit/value.h"
#include "parser/H/core/parser.h"

// Shared shape for upper()/lower(): allocate a same-length copy, then a
// byte-by-byte transform loop advancing two raw pointers (source cursor
// in RSI, dest cursor in RDI) rather than indexing -- same technique
// codegen/C/functions/closures.c's capture-copy loop uses, avoiding any need for a
// scaled-index addressing mode this project's emitter doesn't support.
static void case_transform(char lo, char hi, int8_t delta) {
    emit_pop_reg(code, REG_RAX); // src payload
    emit_pop_reg(code, REG_RBX); // tag (ignored, assumed STRING)
    emit_load_mem_disp32(code, REG_RDX, REG_RAX, -8); // length
    emit_mov_reg_reg(code, REG_RBX, REG_RAX); // stash src base (survives the alloc call)

    string_alloc_emit_prefixed(code); // RAX = new block, RDX = length (preserved)

    emit_mov_reg_reg(code, REG_RSI, REG_RBX); // src cursor
    emit_mov_reg_reg(code, REG_RDI, REG_RAX);
    emit_add_reg_imm8(code, REG_RDI, 8); // dest cursor
    emit_mov_reg_reg(code, REG_RCX, REG_RDX); // remaining count

    int loop_start = code->count;
    emit_cmp_reg_imm32(code, REG_RCX, 0);
    int loop_exit = emit_jcc_rel32(code, COND_E);
    emit_load_byte_reg(code, REG_RBX, REG_RSI); // current byte
    emit_cmp_reg_imm32(code, REG_RBX, lo);
    int too_low = emit_jcc_rel32(code, COND_LT);
    emit_cmp_reg_imm32(code, REG_RBX, hi);
    int too_high = emit_jcc_rel32(code, COND_GT);
    emit_add_reg_imm8(code, REG_RBX, delta);
    int skip = emit_jmp_rel32(code);
    emit_patch_jump(code, too_low);
    emit_patch_jump(code, too_high);
    emit_patch_jump(code, skip);
    emit_store_byte_reg(code, REG_RDI, REG_RBX);
    emit_add_reg_imm8(code, REG_RSI, 1);
    emit_add_reg_imm8(code, REG_RDI, 1);
    emit_dec_reg(code, REG_RCX);
    emit_jmp_back(code, loop_start);
    emit_patch_jump(code, loop_exit);

    emit_add_reg_imm8(code, REG_RAX, 8); // payload
    emit_mov_reg_imm64(code, REG_RBX, TAG_STRING);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}

void codegen_builtin_upper(void) { case_transform('a', 'z', -32); }
void codegen_builtin_lower(void) { case_transform('A', 'Z', 32); }
