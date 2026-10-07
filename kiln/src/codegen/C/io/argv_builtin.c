#include <stdint.h>
#include "codegen/H/io/argv_builtin.h"
#include "codegen/H/io/argv_builtin_internal.h"
#include "codegen/H/strings/string_alloc.h"
#include "codegen/H/emit/bytes.h"
#include "codegen/H/runtime/heap.h"
#include "codegen/H/emit/value.h"
#include "elf/H/elf_writer.h"
#include "parser/H/core/parser.h"
#include "parser/H/declarations/vars.h"
#include "app/H/target.h"

static void codegen_builtin_argv_linux(void);

void codegen_builtin_argv(void) {
    if (kiln_get_target() == KILN_TARGET_WINDOWS) { codegen_builtin_argv_windows(); return; }
    codegen_builtin_argv_linux();
}

// Scratch layout: [0]=argv_base (points at the kernel's saved argc slot,
// so argv[i]'s pointer lives at argv_base+8+8*i) [8]=argc [16]=out_data_ptr
// [24]=i [32]=c_str (current arg's raw null-terminated pointer) [40]=length
static void codegen_builtin_argv_linux(void) {
    int32_t base = higher_order_scratch_offset();

    emit_mov_reg_imm64(code, REG_RSI, KILN_ARGV_ADDR);
    emit_load_mem_disp32(code, REG_RSI, REG_RSI, 0); // RSI = saved original RSP (-> argc)
    emit_load_mem_disp32(code, REG_RCX, REG_RSI, 0); // argc
    emit_store_mem_disp32(code, REG_RBP, base + 0, REG_RSI);
    emit_store_mem_disp32(code, REG_RBP, base + 8, REG_RCX);

    emit_mov_reg_reg(code, REG_RDI, REG_RCX);
    emit_mov_reg_imm64(code, REG_RBX, 16);
    emit_imul_reg_reg(code, REG_RDI, REG_RBX);
    emit_cmp_reg_imm32(code, REG_RDI, 0);
    int nz = emit_jcc_rel32(code, COND_NE);
    emit_mov_reg_imm64(code, REG_RDI, 1);
    emit_patch_jump(code, nz);
    heap_emit_alloc(code); // RAX = out block
    emit_store_mem_disp32(code, REG_RBP, base + 16, REG_RAX);

    emit_mov_reg_imm64(code, REG_RCX, 0);
    emit_store_mem_disp32(code, REG_RBP, base + 24, REG_RCX); // i = 0

    int loop_start = code->count;
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 24);
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 8);
    emit_cmp_reg_reg(code, REG_RCX, REG_RDX);
    int loop_exit = emit_jcc_rel32(code, COND_GE);

    // c_str = *(argv_base + 8 + 8*i)
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 0);
    emit_mov_reg_reg(code, REG_RAX, REG_RCX);
    emit_mov_reg_imm64(code, REG_RDX, 8);
    emit_imul_reg_reg(code, REG_RAX, REG_RDX);
    emit_add_reg_imm8(code, REG_RAX, 8); // skip the argc slot itself
    emit_add_reg_reg(code, REG_RSI, REG_RAX);
    emit_load_mem_disp32(code, REG_RSI, REG_RSI, 0); // the char* value itself
    emit_store_mem_disp32(code, REG_RBP, base + 32, REG_RSI);

    // strlen (C strings from the kernel aren't length-prefixed)
    emit_mov_reg_reg(code, REG_RCX, REG_RSI);
    int strlen_loop = code->count;
    emit_load_byte_reg(code, REG_RAX, REG_RCX);
    emit_cmp_reg_imm32(code, REG_RAX, 0);
    int strlen_done = emit_jcc_rel32(code, COND_E);
    emit_add_reg_imm8(code, REG_RCX, 1);
    emit_jmp_back(code, strlen_loop);
    emit_patch_jump(code, strlen_done);

    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 32); // c_str
    emit_sub_reg_reg(code, REG_RCX, REG_RDX); // length = cursor - c_str
    emit_store_mem_disp32(code, REG_RBP, base + 40, REG_RCX);

    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 40); // length
    string_alloc_emit_prefixed(code); // RAX = new block
    emit_mov_reg_reg(code, REG_RSI, REG_RAX); // stash (bytes_emit_copy preserves RSI)
    emit_mov_reg_reg(code, REG_RDI, REG_RSI);
    emit_add_reg_imm8(code, REG_RDI, 8);
    emit_load_mem_disp32(code, REG_RBX, REG_RBP, base + 32); // src = c_str
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 40); // length
    bytes_emit_copy(code);

    emit_mov_reg_reg(code, REG_RAX, REG_RSI);
    emit_add_reg_imm8(code, REG_RAX, 8); // payload

    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 24); // i
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 16); // out_data_ptr
    emit_mov_reg_reg(code, REG_RDX, REG_RCX);
    emit_mov_reg_imm64(code, REG_RDI, 16);
    emit_imul_reg_reg(code, REG_RDX, REG_RDI);
    emit_add_reg_reg(code, REG_RSI, REG_RDX);
    emit_mov_reg_imm64(code, REG_RDI, TAG_STRING);
    emit_store_mem_disp32(code, REG_RSI, 0, REG_RDI);
    emit_store_mem_disp32(code, REG_RSI, 8, REG_RAX);

    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 24);
    emit_add_reg_imm8(code, REG_RCX, 1);
    emit_store_mem_disp32(code, REG_RBP, base + 24, REG_RCX);
    emit_jmp_back(code, loop_start);
    emit_patch_jump(code, loop_exit);

    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 8); // argc
    emit_mov_reg_imm64(code, REG_RDI, ARRAY_OBJECT_SIZE);
    heap_emit_alloc(code); // RAX = array object
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 16); // out_data_ptr (reload)
    emit_store_mem_disp32(code, REG_RAX, 0, REG_RCX);
    emit_store_mem_disp32(code, REG_RAX, 8, REG_RCX);
    emit_store_mem_disp32(code, REG_RAX, 16, REG_RSI);

    emit_mov_reg_imm64(code, REG_RBX, TAG_ARRAY);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}
