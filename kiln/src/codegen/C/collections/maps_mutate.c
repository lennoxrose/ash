#include "codegen/H/collections/maps.h"
#include "codegen/H/collections/maps_internal.h"
#include "codegen/H/emit/emit_internal.h"
#include "codegen/H/emit/value.h"
#include "codegen/H/runtime/heap.h"
#include "codegen/H/emit/bytes.h"
#include "parser/H/core/parser.h"

static void push_number_local(double v) {
    uint64_t bits;
    __builtin_memcpy(&bits, &v, sizeof(bits));
    emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
    emit_push_reg(code, REG_RBX);
    emit_mov_reg_imm64(code, REG_RAX, bits);
    emit_push_reg(code, REG_RAX);
}

// Scratch layout (base recomputed via `mov reg,rsp` after any call that
// might have clobbered whichever register held it -- same technique
// codegen/C/collections/arrays.c's codegen_builtin_push uses): [0]=val_tag
// [8]=val_payload [16]=key_payload [24]=map_object [32]=new_capacity
// (grow path only) [40]=new_entries_block (grow path only)
void codegen_map_index_store(void) {
    emit_pop_reg(code, REG_RAX); // value payload
    emit_pop_reg(code, REG_RBX); // value tag
    emit_pop_reg(code, REG_RDI); // key payload
    emit_pop_reg(code, REG_RCX); // key tag (ignored)
    emit_pop_reg(code, REG_RSI); // map object
    emit_pop_reg(code, REG_RCX); // map tag (ignored)

    emit_sub_reg_imm8(code, REG_RSP, 48);
    emit_mov_reg_reg(code, REG_RCX, REG_RSP);
    emit_store_mem_disp32(code, REG_RCX, 0, REG_RBX);
    emit_store_mem_disp32(code, REG_RCX, 8, REG_RAX);
    emit_store_mem_disp32(code, REG_RCX, 16, REG_RDI);
    emit_store_mem_disp32(code, REG_RCX, 24, REG_RSI);

    int not_found = find_entry_index();

    // --- found: update value in place ---
    emit_mov_reg_reg(code, REG_RCX, REG_RSP);
    emit_load_mem_disp32(code, REG_RAX, REG_RCX, 0);
    emit_store_mem_disp32(code, REG_RBX, 16, REG_RAX);
    emit_load_mem_disp32(code, REG_RAX, REG_RCX, 8);
    emit_store_mem_disp32(code, REG_RBX, 24, REG_RAX);
    emit_add_reg_imm8(code, REG_RSP, 48);
    int done = emit_jmp_rel32(code);

    // --- not found: append, growing first if at capacity ---
    emit_patch_jump(code, not_found);
    emit_mov_reg_reg(code, REG_RCX, REG_RSP);
    emit_load_mem_disp32(code, REG_RSI, REG_RCX, 24);
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 0); // capacity
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, 8); // count
    emit_cmp_reg_reg(code, REG_RDX, REG_RAX);
    int has_room = emit_jcc_rel32(code, COND_LT);

    emit_add_reg_imm8(code, REG_RAX, 4); // new_capacity
    emit_store_mem_disp32(code, REG_RCX, 32, REG_RAX);
    emit_mov_reg_reg(code, REG_RDI, REG_RAX);
    emit_mov_reg_imm64(code, REG_RBX, MAP_ENTRY_SIZE);
    emit_imul_reg_reg(code, REG_RDI, REG_RBX);
    heap_emit_alloc(code); // RAX = new entries block
    emit_mov_reg_reg(code, REG_RCX, REG_RSP);
    emit_store_mem_disp32(code, REG_RCX, 40, REG_RAX);

    emit_load_mem_disp32(code, REG_RSI, REG_RCX, 24);
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, 0);  // old capacity
    emit_load_mem_disp32(code, REG_RBX, REG_RSI, 16); // old entries_ptr (src)
    emit_load_mem_disp32(code, REG_RDI, REG_RCX, 40); // new block (dest)
    emit_mov_reg_reg(code, REG_RAX, REG_RDX);
    emit_mov_reg_imm64(code, REG_RDX, MAP_ENTRY_SIZE);
    emit_imul_reg_reg(code, REG_RAX, REG_RDX);
    emit_mov_reg_reg(code, REG_RDX, REG_RAX); // byte count
    bytes_emit_copy(code);

    emit_mov_reg_reg(code, REG_RCX, REG_RSP);
    emit_load_mem_disp32(code, REG_RSI, REG_RCX, 24);
    emit_load_mem_disp32(code, REG_RAX, REG_RCX, 32);
    emit_store_mem_disp32(code, REG_RSI, 0, REG_RAX);  // capacity = new_capacity
    emit_load_mem_disp32(code, REG_RAX, REG_RCX, 40);
    emit_store_mem_disp32(code, REG_RSI, 16, REG_RAX); // entries_ptr = new block

    emit_patch_jump(code, has_room);

    emit_mov_reg_reg(code, REG_RCX, REG_RSP);
    emit_load_mem_disp32(code, REG_RSI, REG_RCX, 24);
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, 8);  // count
    emit_load_mem_disp32(code, REG_RDI, REG_RSI, 16); // entries_ptr
    emit_mov_reg_reg(code, REG_RAX, REG_RDX);
    emit_mov_reg_imm64(code, REG_RDX, MAP_ENTRY_SIZE);
    emit_imul_reg_reg(code, REG_RAX, REG_RDX);
    emit_add_reg_reg(code, REG_RAX, REG_RDI); // RAX = new entry's address

    emit_mov_reg_imm64(code, REG_RBX, TAG_STRING);
    emit_store_mem_disp32(code, REG_RAX, 0, REG_RBX);
    emit_load_mem_disp32(code, REG_RBX, REG_RCX, 16); // key_payload
    emit_store_mem_disp32(code, REG_RAX, 8, REG_RBX);
    emit_load_mem_disp32(code, REG_RBX, REG_RCX, 0); // val_tag
    emit_store_mem_disp32(code, REG_RAX, 16, REG_RBX);
    emit_load_mem_disp32(code, REG_RBX, REG_RCX, 8); // val_payload
    emit_store_mem_disp32(code, REG_RAX, 24, REG_RBX);

    emit_load_mem_disp32(code, REG_RBX, REG_RSI, 8);
    emit_add_reg_imm8(code, REG_RBX, 1);
    emit_store_mem_disp32(code, REG_RSI, 8, REG_RBX); // count++

    emit_add_reg_imm8(code, REG_RSP, 48);

    emit_patch_jump(code, done);
}

void codegen_builtin_has(void) {
    emit_pop_reg(code, REG_RDI); // key payload
    emit_pop_reg(code, REG_RBX); // key tag (ignored)
    emit_pop_reg(code, REG_RSI); // map object
    emit_pop_reg(code, REG_RBX); // map tag (ignored)

    int not_found = find_entry_index();
    push_number_local(1.0);
    int done = emit_jmp_rel32(code);
    emit_patch_jump(code, not_found);
    push_number_local(0.0);
    emit_patch_jump(code, done);
}

void codegen_builtin_delete(void) {
    emit_pop_reg(code, REG_RDI); // key payload
    emit_pop_reg(code, REG_RBX); // key tag (ignored)
    emit_pop_reg(code, REG_RSI); // map object
    emit_pop_reg(code, REG_RBX); // map tag (ignored)

    emit_sub_reg_imm8(code, REG_RSP, 8);
    emit_mov_reg_reg(code, REG_RCX, REG_RSP);
    emit_store_mem_disp32(code, REG_RCX, 0, REG_RSI);

    int not_found = find_entry_index();
    // found: RBX = entry addr to remove -- overwrite it with the last
    // entry's data (order doesn't matter for a map), then count--
    emit_mov_reg_reg(code, REG_RCX, REG_RSP);
    emit_load_mem_disp32(code, REG_RSI, REG_RCX, 0);
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 8); // count
    emit_sub_reg_imm8(code, REG_RAX, 1);               // last_index
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, 16); // entries_ptr
    emit_mov_reg_reg(code, REG_RCX, REG_RAX);
    emit_mov_reg_imm64(code, REG_RAX, MAP_ENTRY_SIZE);
    emit_imul_reg_reg(code, REG_RCX, REG_RAX);
    emit_add_reg_reg(code, REG_RCX, REG_RDX); // RCX = last entry's address

    emit_load_mem_disp32(code, REG_RAX, REG_RCX, 0);  emit_store_mem_disp32(code, REG_RBX, 0, REG_RAX);
    emit_load_mem_disp32(code, REG_RAX, REG_RCX, 8);  emit_store_mem_disp32(code, REG_RBX, 8, REG_RAX);
    emit_load_mem_disp32(code, REG_RAX, REG_RCX, 16); emit_store_mem_disp32(code, REG_RBX, 16, REG_RAX);
    emit_load_mem_disp32(code, REG_RAX, REG_RCX, 24); emit_store_mem_disp32(code, REG_RBX, 24, REG_RAX);

    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 8);
    emit_sub_reg_imm8(code, REG_RAX, 1);
    emit_store_mem_disp32(code, REG_RSI, 8, REG_RAX); // count--

    emit_add_reg_imm8(code, REG_RSP, 8);
    emit_mov_reg_imm64(code, REG_RBX, TAG_MAP);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RSI); // delete() returns the map, matching ashvm
    int done = emit_jmp_rel32(code);

    // key absent -- no-op, still return the map (matches ashvm's
    // unconditional "vm_map_delete then return args[0]")
    emit_patch_jump(code, not_found);
    emit_mov_reg_reg(code, REG_RCX, REG_RSP);
    emit_load_mem_disp32(code, REG_RSI, REG_RCX, 0);
    emit_add_reg_imm8(code, REG_RSP, 8);
    emit_mov_reg_imm64(code, REG_RBX, TAG_MAP);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RSI);

    emit_patch_jump(code, done);
}
