#include "codegen/H/strings/string_builtins.h"
#include "codegen/H/strings/string_alloc.h"
#include "codegen/H/runtime/errors.h"
#include "codegen/H/emit/emit_internal.h"
#include "codegen/H/emit/emit_sse.h"
#include "codegen/H/emit/value.h"
#include "parser/H/core/parser.h"

// chr(n): a one-byte string holding byte n (1..255). Same allocation shape as
// codegen_string_index_read's one-char result.
void codegen_builtin_chr(void) {
    emit_pop_reg(code, REG_RAX); // payload
    emit_pop_reg(code, REG_RBX); // tag (ignored, assumed NUMBER)
    emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
    emit_cvttsd2si(code, REG_RBX, XMM0); // code (RBX survives string_alloc_emit_prefixed)

    emit_cmp_reg_imm32(code, REG_RBX, 1);
    int too_small = emit_jcc_rel32(code, COND_LT);
    emit_cmp_reg_imm32(code, REG_RBX, 255);
    int too_big = emit_jcc_rel32(code, COND_GT);
    int ok = emit_jmp_rel32(code);
    emit_patch_jump(code, too_small);
    emit_patch_jump(code, too_big);
    errors_emit_die(code, "runtime error: chr() expects a number from 1 to 255");
    emit_patch_jump(code, ok);

    emit_mov_reg_imm64(code, REG_RDX, 1);
    string_alloc_emit_prefixed(code); // RAX = new 1-byte block, length prefix written
    emit_add_reg_imm8(code, REG_RAX, 8);
    emit_store_byte_reg(code, REG_RAX, REG_RBX);

    emit_mov_reg_imm64(code, REG_RBX, TAG_STRING);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}

// ord(s): the first byte of s as a number (0..255).
void codegen_builtin_ord(void) {
    emit_pop_reg(code, REG_RSI); // string payload
    emit_pop_reg(code, REG_RBX); // tag (ignored, assumed STRING)

    emit_load_mem_disp32(code, REG_RDX, REG_RSI, -8); // length
    emit_cmp_reg_imm32(code, REG_RDX, 0);
    int nonempty = emit_jcc_rel32(code, COND_NE);
    errors_emit_die(code, "runtime error: ord() expects a non-empty string");
    emit_patch_jump(code, nonempty);

    emit_load_byte_reg(code, REG_RAX, REG_RSI);
    emit_cvtsi2sd(code, XMM0, REG_RAX);
    emit_movq_reg_from_xmm(code, REG_RAX, XMM0);
    emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}
