#include <stdint.h>
#include "codegen/H/io/input_builtin.h"
#include "codegen/H/strings/string_alloc.h"
#include "codegen/H/emit/bytes.h"
#include "codegen/H/runtime/heap.h"
#include "codegen/H/emit/value.h"
#include "codegen/H/platform/platform_console.h"
#include "parser/H/core/parser.h"
#include "parser/H/declarations/vars.h"

// Reads one line from stdin (matching ashvm's fgets(buf,1024,stdin) +
// trailing-newline-strip): a fixed 1024-byte cap, same as ashvm's own
// buffer size. No libc getline/fgets in this freestanding binary, so
// bytes come in one at a time via raw read(2) syscalls -- slow, but
// input() is never a hot loop, and "simple and obviously correct" beats
// hand-rolling a buffered reader for this.
void codegen_builtin_input(void) {
    emit_mov_reg_imm64(code, REG_RDI, 1024);
    heap_emit_alloc(code); // RAX = buf
    int32_t base = higher_order_scratch_offset();
    emit_store_mem_disp32(code, REG_RBP, base + 0, REG_RAX); // buf
    emit_mov_reg_imm64(code, REG_RCX, 0);
    emit_store_mem_disp32(code, REG_RBP, base + 8, REG_RCX); // pos = 0

    // Looked up once, reused every iteration -- on Windows this turns a
    // multi-byte line into one GetStdHandle round-trip instead of one per
    // byte (also closes a race window under Wine, where extra round-trips
    // gave another process time to drain the piped input first).
    platform_emit_get_stdin_handle(code);
    emit_store_mem_disp32(code, REG_RBP, base + 16, REG_RAX);

    int end_jumps[3]; int end_count = 0;

    int loop_start = code->count;
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 8);
    emit_cmp_reg_imm32(code, REG_RCX, 1024);
    end_jumps[end_count++] = emit_jcc_rel32(code, COND_GE); // buffer full

    emit_sub_reg_imm8(code, REG_RSP, 8);
    emit_mov_reg_reg(code, REG_RSI, REG_RSP);
    emit_mov_reg_imm64(code, REG_RDX, 1);
    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 16); // cached handle
    platform_emit_read_stdin_byte_h(code);
    emit_cmp_reg_imm32(code, REG_RAX, 1);
    int got_byte = emit_jcc_rel32(code, COND_E);
    emit_add_reg_imm8(code, REG_RSP, 8);
    end_jumps[end_count++] = emit_jmp_rel32(code); // EOF or error
    emit_patch_jump(code, got_byte);

    emit_mov_reg_reg(code, REG_RCX, REG_RSP); // RSP can't be a byte-load base directly (needs a SIB byte this module doesn't emit)
    emit_load_byte_reg(code, REG_RCX, REG_RCX);
    emit_add_reg_imm8(code, REG_RSP, 8);
    emit_cmp_reg_imm32(code, REG_RCX, '\n');
    end_jumps[end_count++] = emit_jcc_rel32(code, COND_E); // newline -- don't store it

    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 0);
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 8);
    emit_add_reg_reg(code, REG_RSI, REG_RDX);
    emit_store_byte_reg(code, REG_RSI, REG_RCX);
    emit_add_reg_imm8(code, REG_RDX, 1);
    emit_store_mem_disp32(code, REG_RBP, base + 8, REG_RDX); // pos++
    emit_jmp_back(code, loop_start);

    for (int i = 0; i < end_count; i++) emit_patch_jump(code, end_jumps[i]);

    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 8); // final length
    string_alloc_emit_prefixed(code); // RAX = block
    emit_mov_reg_reg(code, REG_RSI, REG_RAX); // stash (bytes_emit_copy preserves RSI)
    emit_mov_reg_reg(code, REG_RDI, REG_RSI);
    emit_add_reg_imm8(code, REG_RDI, 8);
    emit_load_mem_disp32(code, REG_RBX, REG_RBP, base + 0); // src = line buffer
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 8); // length
    bytes_emit_copy(code);

    emit_mov_reg_reg(code, REG_RAX, REG_RSI);
    emit_add_reg_imm8(code, REG_RAX, 8); // payload
    emit_mov_reg_imm64(code, REG_RBX, TAG_STRING);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}
