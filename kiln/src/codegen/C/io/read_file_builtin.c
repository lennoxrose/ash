#include <stdint.h>
#include "codegen/H/io/file_builtins.h"
#include "codegen/H/io/file_path.h"
#include "codegen/H/strings/string_alloc.h"
#include "codegen/H/runtime/errors.h"
#include "codegen/H/emit/value.h"
#include "codegen/H/platform/platform_file.h"
#include "parser/H/core/parser.h"
#include "parser/H/declarations/vars.h"

// A single read() call is assumed to fill the whole file in one go (matches
// ashvm's own single fread call, not a retry loop) -- reasonable for
// regular files, not a guarantee POSIX makes for every file type.
void codegen_builtin_read_file(void) {
    emit_pop_reg(code, REG_RSI); // path payload
    emit_pop_reg(code, REG_RBX); // tag (ignored, assumed STRING)
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, -8); // length
    filepath_emit_nullterm(code); // RAX = null-terminated path buffer

    emit_mov_reg_reg(code, REG_RDI, REG_RAX);
    platform_emit_open_read(code);

    emit_cmp_reg_imm32(code, REG_RAX, 0);
    int ok = emit_jcc_rel32(code, COND_GE);
    errors_emit_die(code, "runtime error: could not open file");
    emit_patch_jump(code, ok);

    int32_t base = higher_order_scratch_offset();
    emit_store_mem_disp32(code, REG_RBP, base + 0, REG_RAX); // fd

    emit_mov_reg_reg(code, REG_RDI, REG_RAX);
    platform_emit_file_size(code);
    emit_store_mem_disp32(code, REG_RBP, base + 8, REG_RAX); // size

    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 8); // size
    string_alloc_emit_prefixed(code); // RAX = block, length prefix already written
    emit_store_mem_disp32(code, REG_RBP, base + 16, REG_RAX);

    emit_load_mem_disp32(code, REG_RDI, REG_RBP, base + 0); // fd
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 16);
    emit_add_reg_imm8(code, REG_RSI, 8); // buf = block+8
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 8); // size
    platform_emit_read_bytes(code);

    emit_load_mem_disp32(code, REG_RDI, REG_RBP, base + 0); // fd
    platform_emit_close(code);

    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 16);
    emit_add_reg_imm8(code, REG_RAX, 8); // payload
    emit_mov_reg_imm64(code, REG_RBX, TAG_STRING);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}
