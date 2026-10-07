#include <stdint.h>
#include "codegen/H/strings/string_builtins.h"
#include "codegen/H/strings/string_alloc.h"
#include "codegen/H/expressions/expr.h"
#include "codegen/H/emit/bytes.h"
#include "codegen/H/emit/emit.h"
#include "codegen/H/emit/value.h"
#include "parser/H/core/parser.h"
#include "parser/H/declarations/vars.h"

// Scratch layout: [0]=s_ptr [8]=s_len [16]=search_ptr [24]=search_len
//   [32]=cur [40]=total_len/occurrences [48]=out_block [56]=cursor
//   [64]=find_p [72]=find_last_start [80]=find_result
//   [88]=replacement_ptr [96]=replacement_len

// Same shape as split_builtin.c's helper of the same name (own copy --
// tight enough coupling to this file's scratch layout that sharing it
// wouldn't simplify anything). Searches for [search_ptr,search_len]
// within s starting at [cur]; writes the match position or -1 to
// [base+80].
static void emit_find_delim(int32_t base) {
    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 8);
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 24);
    emit_sub_reg_reg(code, REG_RAX, REG_RDX);
    emit_store_mem_disp32(code, REG_RBP, base + 72, REG_RAX); // last_start

    emit_load_mem_disp32(code, REG_RBX, REG_RBP, base + 32);
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

void codegen_builtin_replace(void) {
    codegen_expression(); // s
    expect(TOKEN_COMMA, "expected ',' after string argument");
    codegen_expression(); // search
    expect(TOKEN_COMMA, "expected ',' after search argument");
    codegen_expression(); // replacement
    expect(TOKEN_RPAREN, "expected ')' after arguments");

    emit_pop_reg(code, REG_RSI); // replacement payload
    emit_pop_reg(code, REG_RBX); // ignored
    int32_t base = higher_order_scratch_offset();
    emit_store_mem_disp32(code, REG_RBP, base + 88, REG_RSI);
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, -8);
    emit_store_mem_disp32(code, REG_RBP, base + 96, REG_RDX); // replacement_len

    emit_pop_reg(code, REG_RCX); // search payload
    emit_pop_reg(code, REG_RBX); // ignored
    emit_store_mem_disp32(code, REG_RBP, base + 16, REG_RCX);
    emit_load_mem_disp32(code, REG_RDX, REG_RCX, -8);
    emit_store_mem_disp32(code, REG_RBP, base + 24, REG_RDX); // search_len

    emit_pop_reg(code, REG_RSI); // s payload
    emit_pop_reg(code, REG_RBX); // ignored
    emit_store_mem_disp32(code, REG_RBP, base + 0, REG_RSI);
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, -8);
    emit_store_mem_disp32(code, REG_RBP, base + 8, REG_RDX); // s_len

    // ---- search is empty: return s unchanged (matches ashvm) ----
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 24); // search_len
    emit_cmp_reg_imm32(code, REG_RDX, 0);
    int not_empty_search = emit_jcc_rel32(code, COND_NE);

    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 8); // s_len
    string_alloc_emit_prefixed(code); // RAX = block
    emit_mov_reg_reg(code, REG_RSI, REG_RAX);
    emit_mov_reg_reg(code, REG_RDI, REG_RSI);
    emit_add_reg_imm8(code, REG_RDI, 8);
    emit_load_mem_disp32(code, REG_RBX, REG_RBP, base + 0); // src = s_ptr
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 8); // len = s_len
    bytes_emit_copy(code);
    emit_mov_reg_reg(code, REG_RAX, REG_RSI);
    emit_add_reg_imm8(code, REG_RAX, 8);
    emit_mov_reg_imm64(code, REG_RBX, TAG_STRING);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);

    int skip_general = emit_jmp_rel32(code);
    emit_patch_jump(code, not_empty_search);

    // ---- pass 1: count occurrences, compute total output length ----
    emit_mov_reg_imm64(code, REG_RAX, 0);
    emit_store_mem_disp32(code, REG_RBP, base + 32, REG_RAX); // cur = 0
    emit_store_mem_disp32(code, REG_RBP, base + 40, REG_RAX); // occurrences = 0

    int p1 = code->count;
    emit_find_delim(base);
    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 80);
    emit_cmp_reg_imm32(code, REG_RAX, -1);
    int p1_done = emit_jcc_rel32(code, COND_E);

    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 40);
    emit_add_reg_imm8(code, REG_RCX, 1);
    emit_store_mem_disp32(code, REG_RBP, base + 40, REG_RCX); // occurrences++
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 24); // search_len
    emit_add_reg_reg(code, REG_RAX, REG_RDX);
    emit_store_mem_disp32(code, REG_RBP, base + 32, REG_RAX); // cur = match + search_len
    emit_jmp_back(code, p1);
    emit_patch_jump(code, p1_done);

    // total_len = s_len + occurrences*(replacement_len - search_len)
    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 96); // replacement_len
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 24); // search_len
    emit_sub_reg_reg(code, REG_RAX, REG_RDX); // delta (signed)
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 40); // occurrences
    emit_imul_reg_reg(code, REG_RAX, REG_RCX); // occurrences * delta
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 8); // s_len
    emit_add_reg_reg(code, REG_RAX, REG_RDX); // total_len

    emit_mov_reg_reg(code, REG_RDX, REG_RAX);
    string_alloc_emit_prefixed(code); // RAX = block
    emit_store_mem_disp32(code, REG_RBP, base + 48, REG_RAX);
    emit_mov_reg_reg(code, REG_RDX, REG_RAX);
    emit_add_reg_imm8(code, REG_RDX, 8);
    emit_store_mem_disp32(code, REG_RBP, base + 56, REG_RDX); // cursor

    // ---- pass 2: copy segments, substituting each match ----
    emit_mov_reg_imm64(code, REG_RAX, 0);
    emit_store_mem_disp32(code, REG_RBP, base + 32, REG_RAX); // cur = 0

    int p2 = code->count;
    emit_find_delim(base);
    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 80);
    emit_cmp_reg_imm32(code, REG_RAX, -1);
    int p2_done = emit_jcc_rel32(code, COND_E);

    // copy s[cur..match) to cursor
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 32); // cur
    emit_mov_reg_reg(code, REG_RDX, REG_RAX);
    emit_sub_reg_reg(code, REG_RDX, REG_RCX); // segment length
    emit_load_mem_disp32(code, REG_RBX, REG_RBP, base + 0);
    emit_add_reg_reg(code, REG_RBX, REG_RCX); // src = s_ptr + cur
    emit_load_mem_disp32(code, REG_RDI, REG_RBP, base + 56); // dest = cursor
    bytes_emit_copy(code);

    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 56);
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 32);
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 80); // match
    emit_sub_reg_reg(code, REG_RDX, REG_RCX);
    emit_add_reg_reg(code, REG_RAX, REG_RDX);
    emit_store_mem_disp32(code, REG_RBP, base + 56, REG_RAX); // cursor advanced

    // copy replacement to cursor
    emit_load_mem_disp32(code, REG_RBX, REG_RBP, base + 88); // replacement_ptr
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 96); // replacement_len
    emit_load_mem_disp32(code, REG_RDI, REG_RBP, base + 56);
    bytes_emit_copy(code);

    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 56);
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 96);
    emit_add_reg_reg(code, REG_RAX, REG_RDX);
    emit_store_mem_disp32(code, REG_RBP, base + 56, REG_RAX); // cursor advanced

    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 80); // match
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 24); // search_len
    emit_add_reg_reg(code, REG_RAX, REG_RDX);
    emit_store_mem_disp32(code, REG_RBP, base + 32, REG_RAX); // cur = match + search_len
    emit_jmp_back(code, p2);
    emit_patch_jump(code, p2_done);

    // trailing remainder s[cur..s_len)
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 32);
    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 8);
    emit_mov_reg_reg(code, REG_RDX, REG_RAX);
    emit_sub_reg_reg(code, REG_RDX, REG_RCX);
    emit_load_mem_disp32(code, REG_RBX, REG_RBP, base + 0);
    emit_add_reg_reg(code, REG_RBX, REG_RCX);
    emit_load_mem_disp32(code, REG_RDI, REG_RBP, base + 56);
    bytes_emit_copy(code);

    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 48); // out block
    emit_add_reg_imm8(code, REG_RAX, 8);
    emit_mov_reg_imm64(code, REG_RBX, TAG_STRING);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);

    emit_patch_jump(code, skip_general);
}
