#include "codegen/H/strings/string_builtins.h"
#include "codegen/H/strings/string_alloc.h"
#include "codegen/H/expressions/expr.h"
#include "codegen/H/emit/emit.h"
#include "codegen/H/emit/emit_sse.h"
#include "codegen/H/emit/bytes.h"
#include "codegen/H/emit/value.h"
#include "codegen/H/runtime/errors.h"
#include "parser/H/core/parser.h"
#include "parser/H/declarations/vars.h"

static void push_number(double v) {
    emit_mov_reg_imm64(code, REG_RBX, TAG_BOOL);
    emit_push_reg(code, REG_RBX);
    uint64_t bits;
    __builtin_memcpy(&bits, &v, sizeof(bits));
    emit_mov_reg_imm64(code, REG_RAX, bits);
    emit_push_reg(code, REG_RAX);
}

// contains(s, search) = indexOf(s, search) >= 0
void codegen_builtin_contains(void) {
    codegen_builtin_indexof();
    emit_pop_reg(code, REG_RAX);
    emit_pop_reg(code, REG_RBX);
    emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
    emit_pxor_xmm_xmm(code, XMM1);
    emit_ucomisd(code, XMM0, XMM1);
    int found = emit_jcc_rel32(code, COND_AE);
    push_number(0.0);
    int done = emit_jmp_rel32(code);
    emit_patch_jump(code, found);
    push_number(1.0);
    emit_patch_jump(code, done);
}

// starts_with(s, prefix) / ends_with(s, suffix): compare `affix_len` bytes at
// the start (or at s_len - affix_len) of s.
static void affix(int at_end) {
    codegen_expression(); // s
    expect(TOKEN_COMMA, "expected ',' after string argument");
    codegen_expression(); // affix
    expect(TOKEN_RPAREN, "expected ')' after arguments");

    emit_pop_reg(code, REG_RCX); // affix payload
    emit_pop_reg(code, REG_RBX);
    emit_pop_reg(code, REG_RSI); // s payload
    emit_pop_reg(code, REG_RBX);
    emit_load_mem_disp32(code, REG_RDX, REG_RCX, -8); // affix_len
    emit_load_mem_disp32(code, REG_RDI, REG_RSI, -8); // s_len
    emit_cmp_reg_reg(code, REG_RDX, REG_RDI);
    int too_long = emit_jcc_rel32(code, COND_GT);
    if (at_end) {
        emit_sub_reg_reg(code, REG_RDI, REG_RDX);
        emit_add_reg_reg(code, REG_RSI, REG_RDI);
    }
    int loop = code->count;
    emit_cmp_reg_imm32(code, REG_RDX, 0);
    int matched = emit_jcc_rel32(code, COND_E);
    emit_load_byte_reg(code, REG_RAX, REG_RSI);
    emit_load_byte_reg(code, REG_RBX, REG_RCX);
    emit_cmp_reg_reg(code, REG_RAX, REG_RBX);
    int mismatch = emit_jcc_rel32(code, COND_NE);
    emit_add_reg_imm8(code, REG_RSI, 1);
    emit_add_reg_imm8(code, REG_RCX, 1);
    emit_dec_reg(code, REG_RDX);
    emit_jmp_back(code, loop);

    emit_patch_jump(code, too_long);
    emit_patch_jump(code, mismatch);
    push_number(0.0);
    int done = emit_jmp_rel32(code);
    emit_patch_jump(code, matched);
    push_number(1.0);
    emit_patch_jump(code, done);
}

void codegen_builtin_starts_with(void) { affix(0); }
void codegen_builtin_ends_with(void) { affix(1); }

// repeat(s, n): s concatenated n times.
// Scratch: [0]=s [8]=len [16]=remaining [24]=dest cursor [32]=block
void codegen_builtin_repeat(void) {
    codegen_expression(); // s
    expect(TOKEN_COMMA, "expected ',' after string argument");
    codegen_expression(); // n
    expect(TOKEN_RPAREN, "expected ')' after arguments");

    int32_t base = higher_order_scratch_offset();
    emit_pop_reg(code, REG_RAX);
    emit_pop_reg(code, REG_RBX);
    emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
    emit_cvttsd2si(code, REG_RCX, XMM0); // n
    emit_pop_reg(code, REG_RSI);
    emit_pop_reg(code, REG_RBX);

    emit_cmp_reg_imm32(code, REG_RCX, 0);
    int n_ok = emit_jcc_rel32(code, COND_GE);
    errors_emit_die(code, "runtime error: repeat() count must not be negative");
    emit_patch_jump(code, n_ok);

    emit_load_mem_disp32(code, REG_RDX, REG_RSI, -8); // len
    emit_store_mem_disp32(code, REG_RBP, base + 0, REG_RSI);
    emit_store_mem_disp32(code, REG_RBP, base + 8, REG_RDX);
    emit_store_mem_disp32(code, REG_RBP, base + 16, REG_RCX);
    emit_imul_reg_reg(code, REG_RDX, REG_RCX); // total length
    string_alloc_emit_prefixed(code);          // RAX = block, RDX = total
    emit_store_mem_disp32(code, REG_RBP, base + 32, REG_RAX);
    emit_add_reg_imm8(code, REG_RAX, 8);
    emit_store_mem_disp32(code, REG_RBP, base + 24, REG_RAX);

    int loop = code->count;
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 16);
    emit_cmp_reg_imm32(code, REG_RCX, 0);
    int finished = emit_jcc_rel32(code, COND_E);
    emit_sub_reg_imm8(code, REG_RCX, 1);
    emit_store_mem_disp32(code, REG_RBP, base + 16, REG_RCX);
    emit_load_mem_disp32(code, REG_RDI, REG_RBP, base + 24);
    emit_load_mem_disp32(code, REG_RBX, REG_RBP, base + 0);
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 8);
    bytes_emit_copy(code);
    emit_load_mem_disp32(code, REG_RDI, REG_RBP, base + 24);
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 8);
    emit_add_reg_reg(code, REG_RDI, REG_RDX);
    emit_store_mem_disp32(code, REG_RBP, base + 24, REG_RDI);
    emit_jmp_back(code, loop);
    emit_patch_jump(code, finished);

    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 32);
    emit_add_reg_imm8(code, REG_RAX, 8);
    emit_mov_reg_imm64(code, REG_RBX, TAG_STRING);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}
