#include "codegen/H/strings/string_builtins.h"
#include "codegen/H/expressions/expr.h"
#include "codegen/H/emit/emit.h"
#include "codegen/H/emit/emit_sse.h"
#include "codegen/H/emit/value.h"
#include "parser/H/core/parser.h"
#include "parser/H/declarations/vars.h"

static void push_number(double v) {
    emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
    emit_push_reg(code, REG_RBX);
    if (v == 0.0) {
        emit_pxor_xmm_xmm(code, XMM0);
        emit_movq_reg_from_xmm(code, REG_RAX, XMM0);
    } else {
        uint64_t bits;
        __builtin_memcpy(&bits, &v, sizeof(bits));
        emit_mov_reg_imm64(code, REG_RAX, bits);
    }
    emit_push_reg(code, REG_RAX);
}

// Scratch layout: [0]=s_ptr [8]=s_len [16]=search_ptr [24]=search_len
//   [32]=i (candidate start position) [40]=last_start
//
// A plain O(n*m) scan (compare the search string against every candidate
// start position) -- matches this project's existing "documented
// simplification for implementation reach" precedent (e.g. maps.c's
// linear-scan lookup instead of a hash table).
void codegen_builtin_indexof(void) {
    codegen_expression(); // s
    expect(TOKEN_COMMA, "expected ',' after string argument");
    codegen_expression(); // search
    expect(TOKEN_RPAREN, "expected ')' after arguments");

    emit_pop_reg(code, REG_RCX); // search payload
    emit_pop_reg(code, REG_RBX); // ignored
    emit_pop_reg(code, REG_RSI); // s payload
    emit_pop_reg(code, REG_RBX); // ignored

    int32_t base = higher_order_scratch_offset();
    emit_store_mem_disp32(code, REG_RBP, base + 0, REG_RSI);
    emit_store_mem_disp32(code, REG_RBP, base + 16, REG_RCX);
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, -8); // s_len
    emit_load_mem_disp32(code, REG_RDI, REG_RCX, -8); // search_len
    emit_store_mem_disp32(code, REG_RBP, base + 8, REG_RDX);
    emit_store_mem_disp32(code, REG_RBP, base + 24, REG_RDI);

    int end_jumps[4]; int end_count = 0;

    emit_cmp_reg_imm32(code, REG_RDI, 0);
    int not_empty = emit_jcc_rel32(code, COND_NE);
    push_number(0.0);
    end_jumps[end_count++] = emit_jmp_rel32(code);
    emit_patch_jump(code, not_empty);

    emit_cmp_reg_reg(code, REG_RDI, REG_RDX);
    int fits = emit_jcc_rel32(code, COND_LE);
    push_number(-1.0);
    end_jumps[end_count++] = emit_jmp_rel32(code);
    emit_patch_jump(code, fits);

    emit_mov_reg_reg(code, REG_RAX, REG_RDX);
    emit_sub_reg_reg(code, REG_RAX, REG_RDI); // last valid start position
    emit_store_mem_disp32(code, REG_RBP, base + 40, REG_RAX);
    emit_mov_reg_imm64(code, REG_RBX, 0);
    emit_store_mem_disp32(code, REG_RBP, base + 32, REG_RBX); // i = 0

    int outer = code->count;
    emit_load_mem_disp32(code, REG_RBX, REG_RBP, base + 32);
    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 40);
    emit_cmp_reg_reg(code, REG_RBX, REG_RAX);
    int not_found = emit_jcc_rel32(code, COND_GT);

    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 0);
    emit_add_reg_reg(code, REG_RSI, REG_RBX); // s + i
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 16); // search_ptr
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 24); // search_len (used as inner counter)

    int inner = code->count;
    emit_cmp_reg_imm32(code, REG_RDX, 0);
    int matched = emit_jcc_rel32(code, COND_E);
    emit_load_byte_reg(code, REG_RDI, REG_RSI);
    emit_load_byte_reg(code, REG_RAX, REG_RCX);
    emit_cmp_reg_reg(code, REG_RDI, REG_RAX);
    int mismatch = emit_jcc_rel32(code, COND_NE);
    emit_add_reg_imm8(code, REG_RSI, 1);
    emit_add_reg_imm8(code, REG_RCX, 1);
    emit_dec_reg(code, REG_RDX);
    emit_jmp_back(code, inner);

    emit_patch_jump(code, mismatch);
    emit_load_mem_disp32(code, REG_RBX, REG_RBP, base + 32);
    emit_add_reg_imm8(code, REG_RBX, 1);
    emit_store_mem_disp32(code, REG_RBP, base + 32, REG_RBX);
    emit_jmp_back(code, outer);

    emit_patch_jump(code, matched);
    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 32);
    emit_cvtsi2sd(code, XMM0, REG_RAX);
    emit_movq_reg_from_xmm(code, REG_RAX, XMM0);
    emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
    end_jumps[end_count++] = emit_jmp_rel32(code);

    emit_patch_jump(code, not_found);
    push_number(-1.0);

    for (int i = 0; i < end_count; i++) emit_patch_jump(code, end_jumps[i]);
}
