#include "codegen/string_builtins.h"
#include "codegen/string_alloc.h"
#include "codegen/expr.h"
#include "codegen/bytes.h"
#include "codegen/emit.h"
#include "codegen/emit_sse.h"
#include "codegen/value.h"
#include "parser/parser.h"

// substring(str, start, end): clamps start>=0, end<=len, start>end->start=end
// (matching ashvm's own clamping exactly, not erroring on out-of-range).
void codegen_builtin_substring(void) {
    codegen_expression(); // str
    expect(TOKEN_COMMA, "expected ',' after string argument");
    codegen_expression(); // start
    expect(TOKEN_COMMA, "expected ',' after start argument");
    codegen_expression(); // end
    expect(TOKEN_RPAREN, "expected ')' after arguments");

    emit_pop_reg(code, REG_RAX); // end payload
    emit_pop_reg(code, REG_RBX); // end tag (ignored)
    emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
    emit_cvttsd2si(code, REG_RDI, XMM0); // end (int)

    emit_pop_reg(code, REG_RAX); // start payload
    emit_pop_reg(code, REG_RBX); // start tag (ignored)
    emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
    emit_cvttsd2si(code, REG_RCX, XMM0); // start (int)

    emit_pop_reg(code, REG_RSI); // str payload
    emit_pop_reg(code, REG_RBX); // str tag (ignored)
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, -8); // slen

    emit_cmp_reg_imm32(code, REG_RCX, 0);
    int start_ok = emit_jcc_rel32(code, COND_GE);
    emit_mov_reg_imm64(code, REG_RCX, 0);
    emit_patch_jump(code, start_ok);

    emit_cmp_reg_reg(code, REG_RDI, REG_RDX);
    int end_ok = emit_jcc_rel32(code, COND_LE);
    emit_mov_reg_reg(code, REG_RDI, REG_RDX);
    emit_patch_jump(code, end_ok);

    emit_cmp_reg_reg(code, REG_RCX, REG_RDI);
    int order_ok = emit_jcc_rel32(code, COND_LE);
    emit_mov_reg_reg(code, REG_RCX, REG_RDI);
    emit_patch_jump(code, order_ok);

    emit_mov_reg_reg(code, REG_RDX, REG_RDI);
    emit_sub_reg_reg(code, REG_RDX, REG_RCX); // length = end - start
    emit_add_reg_reg(code, REG_RSI, REG_RCX); // src = str payload + start
    emit_mov_reg_reg(code, REG_RBX, REG_RSI); // stash src (survives the alloc call)

    string_alloc_emit_prefixed(code); // RAX = new block, RDX = length (preserved)

    emit_mov_reg_reg(code, REG_RSI, REG_RAX); // stash block (bytes_emit_copy preserves RSI)
    emit_mov_reg_reg(code, REG_RDI, REG_RSI);
    emit_add_reg_imm8(code, REG_RDI, 8);
    bytes_emit_copy(code); // RBX=src (already set), RDX=length (already set)

    emit_mov_reg_reg(code, REG_RAX, REG_RSI);
    emit_add_reg_imm8(code, REG_RAX, 8);
    emit_mov_reg_imm64(code, REG_RBX, TAG_STRING);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}
