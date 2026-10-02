#include <stdint.h>
#include "codegen/string_builtins.h"
#include "codegen/string_alloc.h"
#include "codegen/expr.h"
#include "codegen/bytes.h"
#include "codegen/emit.h"
#include "codegen/value.h"
#include "codegen/heap.h"
#include "parser/parser.h"
#include "parser/vars.h"

// Scratch layout: [0]=s_ptr [8]=s_len [16]=delim_ptr [24]=delim_len
//   [32]=cur [40]=count [48]=out_data_ptr [56]=out_index
//   [64]=find_p [72]=find_last_start [80]=find_result  (emit_find_delim's own)
//   [88]=piece_start [96]=piece_len [112]=char byte (per-char branch only)

// Scans candidate start positions from [cur] up to s_len-delim_len,
// comparing byte-by-byte against delim (same O(n*m) shape as
// indexof_builtin.c's search, reimplemented here since it's bounded by a
// caller-supplied `cur` rather than always starting at 0). Writes the
// match position to [base+80], or -1 if none. Called twice -- once to
// count pieces, once to extract them -- so the two passes can never
// disagree about where the boundaries are.
static void emit_find_delim(int32_t base) {
    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 8);
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 24);
    emit_sub_reg_reg(code, REG_RAX, REG_RDX);
    emit_store_mem_disp32(code, REG_RBP, base + 72, REG_RAX); // last_start

    emit_load_mem_disp32(code, REG_RBX, REG_RBP, base + 32); // cur
    emit_store_mem_disp32(code, REG_RBP, base + 64, REG_RBX); // p = cur

    int outer = code->count;
    emit_load_mem_disp32(code, REG_RBX, REG_RBP, base + 64);
    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 72);
    emit_cmp_reg_reg(code, REG_RBX, REG_RAX);
    int no_match = emit_jcc_rel32(code, COND_GT);

    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 0);
    emit_add_reg_reg(code, REG_RSI, REG_RBX);
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 16);
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 24);

    int inner = code->count;
    emit_cmp_reg_imm32(code, REG_RDX, 0);
    int found = emit_jcc_rel32(code, COND_E);
    emit_load_byte_reg(code, REG_RDI, REG_RSI);
    emit_load_byte_reg(code, REG_RAX, REG_RCX);
    emit_cmp_reg_reg(code, REG_RDI, REG_RAX);
    int mismatch = emit_jcc_rel32(code, COND_NE);
    emit_add_reg_imm8(code, REG_RSI, 1);
    emit_add_reg_imm8(code, REG_RCX, 1);
    emit_dec_reg(code, REG_RDX);
    emit_jmp_back(code, inner);

    emit_patch_jump(code, mismatch);
    emit_load_mem_disp32(code, REG_RBX, REG_RBP, base + 64);
    emit_add_reg_imm8(code, REG_RBX, 1);
    emit_store_mem_disp32(code, REG_RBP, base + 64, REG_RBX);
    emit_jmp_back(code, outer);

    emit_patch_jump(code, found);
    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 64);
    emit_store_mem_disp32(code, REG_RBP, base + 80, REG_RAX);
    int done = emit_jmp_rel32(code);

    emit_patch_jump(code, no_match);
    emit_mov_reg_imm64(code, REG_RAX, (uint64_t)(int64_t)-1);
    emit_store_mem_disp32(code, REG_RBP, base + 80, REG_RAX);

    emit_patch_jump(code, done);
}

// Copies [base+96] bytes from [base+88] into a fresh heap string, stores
// it (STRING, payload) into out_data_ptr[out_index] (base+48/[base+56]),
// and increments out_index. Shared by both the delim match-piece and the
// final trailing-piece writes in the general (non-empty delim) branch.
static void emit_store_piece(int32_t base) {
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 96); // len
    string_alloc_emit_prefixed(code); // RAX = block
    emit_mov_reg_reg(code, REG_RSI, REG_RAX); // stash (bytes_emit_copy preserves RSI)
    emit_mov_reg_reg(code, REG_RDI, REG_RSI);
    emit_add_reg_imm8(code, REG_RDI, 8);
    emit_load_mem_disp32(code, REG_RBX, REG_RBP, base + 88); // src
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 96); // len
    bytes_emit_copy(code);

    emit_mov_reg_reg(code, REG_RAX, REG_RSI);
    emit_add_reg_imm8(code, REG_RAX, 8); // payload

    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 56); // out_index
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 48); // out_data_ptr
    emit_mov_reg_reg(code, REG_RDX, REG_RCX);
    emit_mov_reg_imm64(code, REG_RDI, 16);
    emit_imul_reg_reg(code, REG_RDX, REG_RDI);
    emit_add_reg_reg(code, REG_RSI, REG_RDX);
    emit_mov_reg_imm64(code, REG_RDI, TAG_STRING);
    emit_store_mem_disp32(code, REG_RSI, 0, REG_RDI);
    emit_store_mem_disp32(code, REG_RSI, 8, REG_RAX);

    emit_add_reg_imm8(code, REG_RCX, 1);
    emit_store_mem_disp32(code, REG_RBP, base + 56, REG_RCX); // out_index++
}

static void emit_alloc_output_block(int32_t base, Reg count_reg) {
    emit_mov_reg_reg(code, REG_RDI, count_reg);
    emit_mov_reg_imm64(code, REG_RBX, 16);
    emit_imul_reg_reg(code, REG_RDI, REG_RBX);
    emit_cmp_reg_imm32(code, REG_RDI, 0);
    int nz = emit_jcc_rel32(code, COND_NE);
    emit_mov_reg_imm64(code, REG_RDI, 1);
    emit_patch_jump(code, nz);
    heap_emit_alloc(code); // RAX = block
    emit_store_mem_disp32(code, REG_RBP, base + 48, REG_RAX); // out_data_ptr
}

void codegen_builtin_split(void) {
    codegen_expression(); // s
    expect(TOKEN_COMMA, "expected ',' after string argument");
    codegen_expression(); // delim
    expect(TOKEN_RPAREN, "expected ')' after arguments");

    emit_pop_reg(code, REG_RSI); // delim payload
    emit_pop_reg(code, REG_RBX); // ignored
    emit_pop_reg(code, REG_RCX); // s payload
    emit_pop_reg(code, REG_RBX); // ignored

    int32_t base = higher_order_scratch_offset();
    emit_store_mem_disp32(code, REG_RBP, base + 0, REG_RCX);
    emit_load_mem_disp32(code, REG_RAX, REG_RCX, -8);
    emit_store_mem_disp32(code, REG_RBP, base + 8, REG_RAX); // s_len
    emit_store_mem_disp32(code, REG_RBP, base + 16, REG_RSI);
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, -8);
    emit_store_mem_disp32(code, REG_RBP, base + 24, REG_RDX); // delim_len

    emit_cmp_reg_imm32(code, REG_RDX, 0);
    int not_char_split = emit_jcc_rel32(code, COND_NE);

    // ---- delim is empty: split into individual characters ----
    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 8);
    emit_store_mem_disp32(code, REG_RBP, base + 40, REG_RAX); // count = s_len
    emit_alloc_output_block(base, REG_RAX);

    emit_mov_reg_imm64(code, REG_RCX, 0);
    emit_store_mem_disp32(code, REG_RBP, base + 56, REG_RCX); // i = 0 (reuses out_index slot)

    int cloop = code->count;
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 56);
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 40);
    emit_cmp_reg_reg(code, REG_RCX, REG_RDX);
    int cexit = emit_jcc_rel32(code, COND_GE);

    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 0);
    emit_add_reg_reg(code, REG_RSI, REG_RCX); // char address
    emit_load_byte_reg(code, REG_RBX, REG_RSI);
    emit_store_mem_disp32(code, REG_RBP, base + 112, REG_RBX); // stash char byte

    emit_mov_reg_imm64(code, REG_RDX, 1);
    string_alloc_emit_prefixed(code); // RAX = new 1-byte block
    emit_add_reg_imm8(code, REG_RAX, 8); // payload

    emit_load_mem_disp32(code, REG_RBX, REG_RBP, base + 112);
    emit_store_byte_reg(code, REG_RAX, REG_RBX);

    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 56);
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 48);
    emit_mov_reg_reg(code, REG_RDX, REG_RCX);
    emit_mov_reg_imm64(code, REG_RDI, 16);
    emit_imul_reg_reg(code, REG_RDX, REG_RDI);
    emit_add_reg_reg(code, REG_RSI, REG_RDX);
    emit_mov_reg_imm64(code, REG_RDI, TAG_STRING);
    emit_store_mem_disp32(code, REG_RSI, 0, REG_RDI);
    emit_store_mem_disp32(code, REG_RSI, 8, REG_RAX);

    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 56);
    emit_add_reg_imm8(code, REG_RCX, 1);
    emit_store_mem_disp32(code, REG_RBP, base + 56, REG_RCX);
    emit_jmp_back(code, cloop);
    emit_patch_jump(code, cexit);

    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 40); // count
    emit_mov_reg_imm64(code, REG_RDI, ARRAY_OBJECT_SIZE);
    heap_emit_alloc(code); // RAX = array object
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 48);
    emit_store_mem_disp32(code, REG_RAX, 0, REG_RCX);
    emit_store_mem_disp32(code, REG_RAX, 8, REG_RCX);
    emit_store_mem_disp32(code, REG_RAX, 16, REG_RSI);
    emit_mov_reg_imm64(code, REG_RBX, TAG_ARRAY);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);

    int skip_general = emit_jmp_rel32(code);
    emit_patch_jump(code, not_char_split);

    // ---- general branch: non-empty delim, two passes ----
    emit_mov_reg_imm64(code, REG_RAX, 0);
    emit_store_mem_disp32(code, REG_RBP, base + 32, REG_RAX); // cur = 0
    emit_store_mem_disp32(code, REG_RBP, base + 40, REG_RAX); // count = 0

    int p1 = code->count;
    emit_find_delim(base);
    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 80);
    emit_cmp_reg_imm32(code, REG_RAX, -1);
    int p1_no_more = emit_jcc_rel32(code, COND_E);

    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 40);
    emit_add_reg_imm8(code, REG_RCX, 1);
    emit_store_mem_disp32(code, REG_RBP, base + 40, REG_RCX); // count++
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 24);
    emit_add_reg_reg(code, REG_RAX, REG_RDX);
    emit_store_mem_disp32(code, REG_RBP, base + 32, REG_RAX); // cur = match + delim_len
    emit_jmp_back(code, p1);

    emit_patch_jump(code, p1_no_more);
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 40);
    emit_add_reg_imm8(code, REG_RCX, 1);
    emit_store_mem_disp32(code, REG_RBP, base + 40, REG_RCX); // + trailing piece

    emit_alloc_output_block(base, REG_RCX);

    emit_mov_reg_imm64(code, REG_RAX, 0);
    emit_store_mem_disp32(code, REG_RBP, base + 32, REG_RAX); // cur = 0
    emit_store_mem_disp32(code, REG_RBP, base + 56, REG_RAX); // out_index = 0

    int p2 = code->count;
    emit_find_delim(base);
    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 80);
    emit_cmp_reg_imm32(code, REG_RAX, -1);
    int p2_no_more = emit_jcc_rel32(code, COND_E);

    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 32); // cur
    emit_mov_reg_reg(code, REG_RDX, REG_RAX);
    emit_sub_reg_reg(code, REG_RDX, REG_RCX); // piece_len
    emit_store_mem_disp32(code, REG_RBP, base + 96, REG_RDX);
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 0);
    emit_add_reg_reg(code, REG_RSI, REG_RCX); // piece_start
    emit_store_mem_disp32(code, REG_RBP, base + 88, REG_RSI);
    emit_store_piece(base);

    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 80); // match
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 24);
    emit_add_reg_reg(code, REG_RAX, REG_RDX);
    emit_store_mem_disp32(code, REG_RBP, base + 32, REG_RAX); // cur = match + delim_len
    emit_jmp_back(code, p2);

    emit_patch_jump(code, p2_no_more);
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 32); // cur
    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 8);  // s_len
    emit_mov_reg_reg(code, REG_RDX, REG_RAX);
    emit_sub_reg_reg(code, REG_RDX, REG_RCX); // trailing piece_len
    emit_store_mem_disp32(code, REG_RBP, base + 96, REG_RDX);
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 0);
    emit_add_reg_reg(code, REG_RSI, REG_RCX);
    emit_store_mem_disp32(code, REG_RBP, base + 88, REG_RSI);
    emit_store_piece(base);

    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 40); // count
    emit_mov_reg_imm64(code, REG_RDI, ARRAY_OBJECT_SIZE);
    heap_emit_alloc(code); // RAX = array object
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 48);
    emit_store_mem_disp32(code, REG_RAX, 0, REG_RCX);
    emit_store_mem_disp32(code, REG_RAX, 8, REG_RCX);
    emit_store_mem_disp32(code, REG_RAX, 16, REG_RSI);
    emit_mov_reg_imm64(code, REG_RBX, TAG_ARRAY);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);

    emit_patch_jump(code, skip_general);
}
