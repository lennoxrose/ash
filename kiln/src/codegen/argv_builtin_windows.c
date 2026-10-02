#include <stdint.h>
#include "codegen/argv_builtin_internal.h"
#include "codegen/string_alloc.h"
#include "codegen/bytes.h"
#include "codegen/heap.h"
#include "codegen/value.h"
#include "codegen/win_call.h"
#include "parser/parser.h"
#include "parser/vars.h"

// GetCommandLineA() returns the whole command line as one string (e.g.
// `prog.exe arg1 arg2`), not a pre-split argv array -- so unlike the
// Linux path (which reads an already-split array straight from the
// kernel's process-startup stack), this has to tokenize it by hand.
// Simple whitespace splitting only, no quoted-argument handling (an
// argument containing a space can't be represented) -- documented
// simplification, same spirit as this project's other "don't
// over-engineer for what can't practically happen" scope limits.
//
// Scratch layout: [0]=cmdline [8]=argc [16]=out_data_ptr [24]=i
// [32]=token_start [40]=token_end [48]=token_len [56]=copy_start
// [64]=copy_end (the latter two are token_start/end with surrounding
// quotes stripped, kept separate so stripping doesn't corrupt the
// resume-scanning position -- see the strip logic below)
void codegen_builtin_argv_windows(void) {
    int32_t base = higher_order_scratch_offset();

    win_call_begin(code, 0);
    win_call_import(code, PE_IMPORT_GET_COMMAND_LINE_A);
    win_call_end(code);
    emit_store_mem_disp32(code, REG_RBP, base + 0, REG_RAX); // cmdline

    // ---- pass 1: count whitespace-separated tokens ----
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 0);
    emit_mov_reg_imm64(code, REG_RDX, 0); // count

    int p1_loop = code->count;
    int p1_skip = code->count;
    emit_load_byte_reg(code, REG_RAX, REG_RCX);
    emit_cmp_reg_imm32(code, REG_RAX, ' ');
    int p1_not_space = emit_jcc_rel32(code, COND_NE);
    emit_add_reg_imm8(code, REG_RCX, 1);
    emit_jmp_back(code, p1_skip);
    emit_patch_jump(code, p1_not_space);
    emit_cmp_reg_imm32(code, REG_RAX, 0);
    int p1_done = emit_jcc_rel32(code, COND_E);
    emit_add_reg_imm8(code, REG_RDX, 1);

    int p1_word = code->count;
    emit_load_byte_reg(code, REG_RAX, REG_RCX);
    emit_cmp_reg_imm32(code, REG_RAX, 0);
    int p1_end1 = emit_jcc_rel32(code, COND_E);
    emit_cmp_reg_imm32(code, REG_RAX, ' ');
    int p1_end2 = emit_jcc_rel32(code, COND_E);
    emit_add_reg_imm8(code, REG_RCX, 1);
    emit_jmp_back(code, p1_word);
    emit_patch_jump(code, p1_end1);
    emit_patch_jump(code, p1_end2);
    emit_jmp_back(code, p1_loop);
    emit_patch_jump(code, p1_done);
    emit_store_mem_disp32(code, REG_RBP, base + 8, REG_RDX); // argc

    // ---- allocate array data ----
    emit_mov_reg_reg(code, REG_RAX, REG_RDX);
    emit_mov_reg_imm64(code, REG_RBX, 16);
    emit_imul_reg_reg(code, REG_RAX, REG_RBX);
    emit_mov_reg_reg(code, REG_RDI, REG_RAX);
    emit_cmp_reg_imm32(code, REG_RDI, 0);
    int nz = emit_jcc_rel32(code, COND_NE);
    emit_mov_reg_imm64(code, REG_RDI, 1);
    emit_patch_jump(code, nz);
    heap_emit_alloc(code); // RAX = out block
    emit_store_mem_disp32(code, REG_RBP, base + 16, REG_RAX);

    // ---- pass 2: build each token as a string, fill the array ----
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 0);
    emit_mov_reg_imm64(code, REG_RSI, 0);
    emit_store_mem_disp32(code, REG_RBP, base + 24, REG_RSI); // i = 0

    int p2_loop = code->count;
    int p2_skip = code->count;
    emit_load_byte_reg(code, REG_RAX, REG_RCX);
    emit_cmp_reg_imm32(code, REG_RAX, ' ');
    int p2_not_space = emit_jcc_rel32(code, COND_NE);
    emit_add_reg_imm8(code, REG_RCX, 1);
    emit_jmp_back(code, p2_skip);
    emit_patch_jump(code, p2_not_space);
    emit_cmp_reg_imm32(code, REG_RAX, 0);
    int p2_done = emit_jcc_rel32(code, COND_E);
    emit_store_mem_disp32(code, REG_RBP, base + 32, REG_RCX); // token_start

    int p2_word = code->count;
    emit_load_byte_reg(code, REG_RAX, REG_RCX);
    emit_cmp_reg_imm32(code, REG_RAX, 0);
    int p2_end1 = emit_jcc_rel32(code, COND_E);
    emit_cmp_reg_imm32(code, REG_RAX, ' ');
    int p2_end2 = emit_jcc_rel32(code, COND_E);
    emit_add_reg_imm8(code, REG_RCX, 1);
    emit_jmp_back(code, p2_word);
    emit_patch_jump(code, p2_end1);
    emit_patch_jump(code, p2_end2);
    emit_store_mem_disp32(code, REG_RBP, base + 40, REG_RCX); // token_end

    // GetCommandLineA() wraps the executable path in quotes (standard
    // Windows convention, always applied by real programs too, not just
    // when the path has spaces) -- strip a leading+trailing '"' pair if
    // both are present on this token. Doesn't handle quoted arguments
    // with embedded spaces (still out of scope, see the file header).
    // Adjusted bounds go in their OWN slots (copy_start/copy_end,
    // +56/+64) rather than overwriting token_start/token_end (+32/+40):
    // the outer loop reloads token_end below to resume scanning for the
    // NEXT token, and stripping a trailing quote there would skip past
    // it into whatever follows, corrupting where the next token starts.
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 32); // token_start
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 40); // token_end
    emit_store_mem_disp32(code, REG_RBP, base + 56, REG_RCX); // copy_start = token_start
    emit_store_mem_disp32(code, REG_RBP, base + 64, REG_RDX); // copy_end = token_end

    emit_load_byte_reg(code, REG_RAX, REG_RCX);
    emit_cmp_reg_imm32(code, REG_RAX, '"');
    int no_open_quote = emit_jcc_rel32(code, COND_NE);
    emit_mov_reg_reg(code, REG_RAX, REG_RDX);
    emit_sub_reg_imm8(code, REG_RAX, 1);
    emit_load_byte_reg(code, REG_RAX, REG_RAX);
    emit_cmp_reg_imm32(code, REG_RAX, '"');
    int no_close_quote = emit_jcc_rel32(code, COND_NE);
    emit_add_reg_imm8(code, REG_RCX, 1);
    emit_store_mem_disp32(code, REG_RBP, base + 56, REG_RCX); // copy_start++
    emit_sub_reg_imm8(code, REG_RDX, 1);
    emit_store_mem_disp32(code, REG_RBP, base + 64, REG_RDX); // copy_end--
    emit_patch_jump(code, no_close_quote);
    emit_patch_jump(code, no_open_quote);

    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 64);
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 56);
    emit_sub_reg_reg(code, REG_RAX, REG_RDX); // len = copy_end - copy_start
    emit_store_mem_disp32(code, REG_RBP, base + 48, REG_RAX);

    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 48);
    string_alloc_emit_prefixed(code); // RAX = new block
    emit_mov_reg_reg(code, REG_RSI, REG_RAX); // stash (bytes_emit_copy preserves RSI)
    emit_mov_reg_reg(code, REG_RDI, REG_RSI);
    emit_add_reg_imm8(code, REG_RDI, 8);
    emit_load_mem_disp32(code, REG_RBX, REG_RBP, base + 56); // src = copy_start
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 48);
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
    emit_store_mem_disp32(code, REG_RBP, base + 24, REG_RCX); // i++

    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 40); // resume from token_end
    emit_jmp_back(code, p2_loop);
    emit_patch_jump(code, p2_done);

    // ---- build the array object ----
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 8); // argc
    emit_mov_reg_imm64(code, REG_RDI, ARRAY_OBJECT_SIZE);
    heap_emit_alloc(code); // RAX = array object
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 16);
    emit_store_mem_disp32(code, REG_RAX, 0, REG_RCX);
    emit_store_mem_disp32(code, REG_RAX, 8, REG_RCX);
    emit_store_mem_disp32(code, REG_RAX, 16, REG_RSI);

    emit_mov_reg_imm64(code, REG_RBX, TAG_ARRAY);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}
