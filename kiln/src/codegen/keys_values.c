#include "codegen/keys_values.h"
#include "codegen/emit.h"
#include "codegen/value.h"
#include "codegen/heap.h"
#include "parser/parser.h"
#include "parser/vars.h"

// This implementation's map (unlike ashvm's capacity-sized hash table
// with scattered "used" flags) keeps every live entry packed into
// [0, count) -- delete() swaps the last entry into a removed slot rather
// than leaving a hole (see codegen/maps_mutate.c) -- so both builtins
// below can just iterate 0..count directly, no "used" check needed.
//
// Scratch layout: [0]=count [8]=entries_ptr [16]=out_block [24]=i

void codegen_builtin_keys(void) {
    emit_pop_reg(code, REG_RSI); // map object
    emit_pop_reg(code, REG_RBX); // map tag (ignored)

    int32_t base = higher_order_scratch_offset();

    emit_load_mem_disp32(code, REG_RCX, REG_RSI, 8);  // count
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, 16); // entries_ptr
    emit_store_mem_disp32(code, REG_RBP, base + 0, REG_RCX);
    emit_store_mem_disp32(code, REG_RBP, base + 8, REG_RDX);

    emit_mov_reg_reg(code, REG_RDI, REG_RCX);
    emit_mov_reg_imm64(code, REG_RBX, 16);
    emit_imul_reg_reg(code, REG_RDI, REG_RBX);
    emit_cmp_reg_imm32(code, REG_RDI, 0);
    int nonzero = emit_jcc_rel32(code, COND_NE);
    emit_mov_reg_imm64(code, REG_RDI, 1);
    emit_patch_jump(code, nonzero);
    heap_emit_alloc(code); // RAX = out block
    emit_store_mem_disp32(code, REG_RBP, base + 16, REG_RAX);

    emit_mov_reg_imm64(code, REG_RCX, 0);
    emit_store_mem_disp32(code, REG_RBP, base + 24, REG_RCX); // i = 0

    int loop_start = code->count;
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 24); // i
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 0);  // count
    emit_cmp_reg_reg(code, REG_RCX, REG_RDX);
    int loop_exit = emit_jcc_rel32(code, COND_GE);

    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 8); // entries_ptr
    emit_mov_reg_reg(code, REG_RAX, REG_RCX);
    emit_mov_reg_imm64(code, REG_RDX, MAP_ENTRY_SIZE);
    emit_imul_reg_reg(code, REG_RAX, REG_RDX);
    emit_add_reg_reg(code, REG_RSI, REG_RAX); // entry address

    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 8); // key payload (keys are always strings)

    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 16); // out_block
    emit_mov_reg_reg(code, REG_RDX, REG_RCX);
    emit_mov_reg_imm64(code, REG_RDI, 16);
    emit_imul_reg_reg(code, REG_RDX, REG_RDI);
    emit_add_reg_reg(code, REG_RSI, REG_RDX); // out slot address

    emit_mov_reg_imm64(code, REG_RDI, TAG_STRING);
    emit_store_mem_disp32(code, REG_RSI, 0, REG_RDI);
    emit_store_mem_disp32(code, REG_RSI, 8, REG_RAX);

    emit_add_reg_imm8(code, REG_RCX, 1);
    emit_store_mem_disp32(code, REG_RBP, base + 24, REG_RCX);
    emit_jmp_back(code, loop_start);
    emit_patch_jump(code, loop_exit);

    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 0); // count
    emit_mov_reg_imm64(code, REG_RDI, ARRAY_OBJECT_SIZE);
    heap_emit_alloc(code); // RAX = array object
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 16); // out block (reload -- alloc clobbers RSI)
    emit_store_mem_disp32(code, REG_RAX, 0, REG_RCX);
    emit_store_mem_disp32(code, REG_RAX, 8, REG_RCX);
    emit_store_mem_disp32(code, REG_RAX, 16, REG_RSI);

    emit_mov_reg_imm64(code, REG_RBX, TAG_ARRAY);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}

void codegen_builtin_values(void) {
    emit_pop_reg(code, REG_RSI); // map object
    emit_pop_reg(code, REG_RBX); // map tag (ignored)

    int32_t base = higher_order_scratch_offset();

    emit_load_mem_disp32(code, REG_RCX, REG_RSI, 8);  // count
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, 16); // entries_ptr
    emit_store_mem_disp32(code, REG_RBP, base + 0, REG_RCX);
    emit_store_mem_disp32(code, REG_RBP, base + 8, REG_RDX);

    emit_mov_reg_reg(code, REG_RDI, REG_RCX);
    emit_mov_reg_imm64(code, REG_RBX, 16);
    emit_imul_reg_reg(code, REG_RDI, REG_RBX);
    emit_cmp_reg_imm32(code, REG_RDI, 0);
    int nonzero = emit_jcc_rel32(code, COND_NE);
    emit_mov_reg_imm64(code, REG_RDI, 1);
    emit_patch_jump(code, nonzero);
    heap_emit_alloc(code); // RAX = out block
    emit_store_mem_disp32(code, REG_RBP, base + 16, REG_RAX);

    emit_mov_reg_imm64(code, REG_RCX, 0);
    emit_store_mem_disp32(code, REG_RBP, base + 24, REG_RCX); // i = 0

    int loop_start = code->count;
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 24); // i
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 0);  // count
    emit_cmp_reg_reg(code, REG_RCX, REG_RDX);
    int loop_exit = emit_jcc_rel32(code, COND_GE);

    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 8); // entries_ptr
    emit_mov_reg_reg(code, REG_RAX, REG_RCX);
    emit_mov_reg_imm64(code, REG_RDX, MAP_ENTRY_SIZE);
    emit_imul_reg_reg(code, REG_RAX, REG_RDX);
    emit_add_reg_reg(code, REG_RSI, REG_RAX); // entry address

    emit_load_mem_disp32(code, REG_RDI, REG_RSI, 16); // value tag
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 24); // value payload

    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 16); // out_block
    emit_mov_reg_reg(code, REG_RDX, REG_RCX);
    emit_mov_reg_imm64(code, REG_RBX, 16);
    emit_imul_reg_reg(code, REG_RDX, REG_RBX);
    emit_add_reg_reg(code, REG_RSI, REG_RDX); // out slot address

    emit_store_mem_disp32(code, REG_RSI, 0, REG_RDI);
    emit_store_mem_disp32(code, REG_RSI, 8, REG_RAX);

    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 24);
    emit_add_reg_imm8(code, REG_RCX, 1);
    emit_store_mem_disp32(code, REG_RBP, base + 24, REG_RCX);
    emit_jmp_back(code, loop_start);
    emit_patch_jump(code, loop_exit);

    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 0); // count
    emit_mov_reg_imm64(code, REG_RDI, ARRAY_OBJECT_SIZE);
    heap_emit_alloc(code); // RAX = array object
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 16); // out block (reload)
    emit_store_mem_disp32(code, REG_RAX, 0, REG_RCX);
    emit_store_mem_disp32(code, REG_RAX, 8, REG_RCX);
    emit_store_mem_disp32(code, REG_RAX, 16, REG_RSI);

    emit_mov_reg_imm64(code, REG_RBX, TAG_ARRAY);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}
