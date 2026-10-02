#include "codegen/string_builtins.h"
#include "codegen/string_alloc.h"
#include "codegen/expr.h"
#include "codegen/bytes.h"
#include "codegen/emit.h"
#include "codegen/value.h"
#include "parser/parser.h"
#include "parser/vars.h"

// Two-pass: pass 1 sums each element's length (+ delim length between
// consecutive elements) to get the exact output size; pass 2 allocates
// once and copies. Avoids any resize-while-building complexity.
//
// Scratch layout: [0]=data_ptr [8]=count [16]=delim_ptr [24]=delim_len
//   [32]=total_len [40]=i [48]=out_block [56]=cursor [64]=elem_len (temp)
void codegen_builtin_join(void) {
    codegen_expression(); // array
    expect(TOKEN_COMMA, "expected ',' after array argument");
    codegen_expression(); // delim
    expect(TOKEN_RPAREN, "expected ')' after arguments");

    emit_pop_reg(code, REG_RSI); // delim payload
    emit_pop_reg(code, REG_RBX); // delim tag (ignored)
    emit_pop_reg(code, REG_RCX); // array object
    emit_pop_reg(code, REG_RBX); // array tag (ignored)

    int32_t base = higher_order_scratch_offset();
    emit_store_mem_disp32(code, REG_RBP, base + 16, REG_RSI); // delim_ptr
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, -8); // delim_len
    emit_store_mem_disp32(code, REG_RBP, base + 24, REG_RDX);

    emit_load_mem_disp32(code, REG_RAX, REG_RCX, 8);  // count
    emit_load_mem_disp32(code, REG_RDX, REG_RCX, 16); // data_ptr
    emit_store_mem_disp32(code, REG_RBP, base + 0, REG_RDX);
    emit_store_mem_disp32(code, REG_RBP, base + 8, REG_RAX);

    // ---- pass 1: total length ----
    emit_mov_reg_imm64(code, REG_RAX, 0);
    emit_store_mem_disp32(code, REG_RBP, base + 32, REG_RAX); // total = 0
    emit_store_mem_disp32(code, REG_RBP, base + 40, REG_RAX); // i = 0

    int p1 = code->count;
    emit_load_mem_disp32(code, REG_RBX, REG_RBP, base + 40);
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 8);
    emit_cmp_reg_reg(code, REG_RBX, REG_RCX);
    int p1_exit = emit_jcc_rel32(code, COND_GE);

    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 0);
    emit_mov_reg_reg(code, REG_RAX, REG_RBX);
    emit_mov_reg_imm64(code, REG_RDX, 16);
    emit_imul_reg_reg(code, REG_RAX, REG_RDX);
    emit_add_reg_reg(code, REG_RSI, REG_RAX);
    emit_load_mem_disp32(code, REG_RSI, REG_RSI, 8); // elem payload (assumed STRING)
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, -8); // elem len

    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 32);
    emit_add_reg_reg(code, REG_RDX, REG_RAX);
    emit_store_mem_disp32(code, REG_RBP, base + 32, REG_RDX); // total += elem_len

    emit_mov_reg_reg(code, REG_RAX, REG_RCX);
    emit_sub_reg_imm8(code, REG_RAX, 1);
    emit_cmp_reg_reg(code, REG_RBX, REG_RAX);
    int last1 = emit_jcc_rel32(code, COND_GE); // last element -> no trailing delim
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 32);
    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 24);
    emit_add_reg_reg(code, REG_RDX, REG_RAX);
    emit_store_mem_disp32(code, REG_RBP, base + 32, REG_RDX);
    emit_patch_jump(code, last1);

    emit_load_mem_disp32(code, REG_RBX, REG_RBP, base + 40);
    emit_add_reg_imm8(code, REG_RBX, 1);
    emit_store_mem_disp32(code, REG_RBP, base + 40, REG_RBX);
    emit_jmp_back(code, p1);
    emit_patch_jump(code, p1_exit);

    // ---- allocate output ----
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 32); // total_len
    string_alloc_emit_prefixed(code); // RAX = block
    emit_store_mem_disp32(code, REG_RBP, base + 48, REG_RAX); // out_block
    emit_mov_reg_reg(code, REG_RDX, REG_RAX);
    emit_add_reg_imm8(code, REG_RDX, 8);
    emit_store_mem_disp32(code, REG_RBP, base + 56, REG_RDX); // cursor = payload start

    // ---- pass 2: copy ----
    emit_mov_reg_imm64(code, REG_RCX, 0);
    emit_store_mem_disp32(code, REG_RBP, base + 40, REG_RCX); // i = 0

    int p2 = code->count;
    emit_load_mem_disp32(code, REG_RBX, REG_RBP, base + 40);
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 8);
    emit_cmp_reg_reg(code, REG_RBX, REG_RCX);
    int p2_exit = emit_jcc_rel32(code, COND_GE);

    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 0);
    emit_mov_reg_reg(code, REG_RAX, REG_RBX);
    emit_mov_reg_imm64(code, REG_RDX, 16);
    emit_imul_reg_reg(code, REG_RAX, REG_RDX);
    emit_add_reg_reg(code, REG_RSI, REG_RAX);
    emit_load_mem_disp32(code, REG_RSI, REG_RSI, 8); // elem payload
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, -8); // elem len
    emit_store_mem_disp32(code, REG_RBP, base + 64, REG_RDX); // stash elem_len

    emit_mov_reg_reg(code, REG_RBX, REG_RSI); // src
    emit_load_mem_disp32(code, REG_RDI, REG_RBP, base + 56); // dest = cursor
    bytes_emit_copy(code);

    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 56);
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 64);
    emit_add_reg_reg(code, REG_RAX, REG_RDX);
    emit_store_mem_disp32(code, REG_RBP, base + 56, REG_RAX); // cursor += elem_len

    emit_load_mem_disp32(code, REG_RBX, REG_RBP, base + 40);
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 8);
    emit_mov_reg_reg(code, REG_RAX, REG_RCX);
    emit_sub_reg_imm8(code, REG_RAX, 1);
    emit_cmp_reg_reg(code, REG_RBX, REG_RAX);
    int last2 = emit_jcc_rel32(code, COND_GE);

    emit_load_mem_disp32(code, REG_RBX, REG_RBP, base + 16); // delim_ptr (src)
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 24); // delim_len
    emit_load_mem_disp32(code, REG_RDI, REG_RBP, base + 56); // cursor (dest)
    bytes_emit_copy(code);

    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 56);
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 24);
    emit_add_reg_reg(code, REG_RAX, REG_RDX);
    emit_store_mem_disp32(code, REG_RBP, base + 56, REG_RAX);

    emit_patch_jump(code, last2);

    emit_load_mem_disp32(code, REG_RBX, REG_RBP, base + 40);
    emit_add_reg_imm8(code, REG_RBX, 1);
    emit_store_mem_disp32(code, REG_RBP, base + 40, REG_RBX);
    emit_jmp_back(code, p2);
    emit_patch_jump(code, p2_exit);

    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 48); // out block
    emit_add_reg_imm8(code, REG_RAX, 8); // payload
    emit_mov_reg_imm64(code, REG_RBX, TAG_STRING);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}
