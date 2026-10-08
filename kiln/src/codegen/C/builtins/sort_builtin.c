#include "codegen/H/builtins/higher_order.h"
#include "codegen/H/expressions/expr.h"
#include "codegen/H/functions/closures.h"
#include "codegen/H/strings/strings.h"
#include "codegen/H/emit/emit.h"
#include "codegen/H/emit/emit_sse.h"
#include "codegen/H/emit/value.h"
#include "parser/H/core/parser.h"
#include "parser/H/declarations/vars.h"

// sort(array) / sort(array, cmp): in-place, stable insertion sort; pushes the
// array back (like push()). Without cmp, numbers sort numerically and strings
// bytewise (a pair of strings -> string order, anything else -> numeric). With
// cmp, an element moves left while cmp(previous, key) is a number > 0.
//
// Scratch layout (see vars.h's higher_order_scratch_offset):
//   [0]=cmp_tag [8]=cmp_payload [16]=data_ptr [24]=count [32]=i [40]=j
//   [48]=key_tag [56]=key_payload [64]=array_object
void codegen_builtin_sort(void) {
    codegen_expression(); // array
    int has_cmp = 0;
    if (current.type == TOKEN_COMMA) {
        advance_token();
        codegen_expression(); // cmp
        has_cmp = 1;
    }
    expect(TOKEN_RPAREN, "expected ')' after arguments");

    int32_t base = higher_order_scratch_offset();

    if (has_cmp) {
        emit_pop_reg(code, REG_RAX);
        emit_pop_reg(code, REG_RBX);
        emit_store_mem_disp32(code, REG_RBP, base + 0, REG_RBX);
        emit_store_mem_disp32(code, REG_RBP, base + 8, REG_RAX);
    }
    emit_pop_reg(code, REG_RSI); // array object
    emit_pop_reg(code, REG_RCX); // tag (ignored)
    emit_store_mem_disp32(code, REG_RBP, base + 64, REG_RSI);
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 8);
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, 16);
    emit_store_mem_disp32(code, REG_RBP, base + 24, REG_RAX);
    emit_store_mem_disp32(code, REG_RBP, base + 16, REG_RDX);
    emit_mov_reg_imm64(code, REG_RCX, 1);
    emit_store_mem_disp32(code, REG_RBP, base + 32, REG_RCX); // i = 1

    int outer = code->count;
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 32);
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 24);
    emit_cmp_reg_reg(code, REG_RCX, REG_RDX);
    int all_done = emit_jcc_rel32(code, COND_GE);

    // key = data[i]; j = i
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 16);
    emit_mov_reg_imm64(code, REG_RAX, 16);
    emit_imul_reg_reg(code, REG_RCX, REG_RAX);
    emit_add_reg_reg(code, REG_RSI, REG_RCX);
    emit_load_mem_disp32(code, REG_RBX, REG_RSI, 0);
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 8);
    emit_store_mem_disp32(code, REG_RBP, base + 48, REG_RBX);
    emit_store_mem_disp32(code, REG_RBP, base + 56, REG_RAX);
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 32);
    emit_store_mem_disp32(code, REG_RBP, base + 40, REG_RCX);

    int inner = code->count;
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 40);
    emit_cmp_reg_imm32(code, REG_RCX, 0);
    int place = emit_jcc_rel32(code, COND_LE);

    // RSI = &data[j - 1]
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 16);
    emit_sub_reg_imm8(code, REG_RCX, 1);
    emit_mov_reg_imm64(code, REG_RAX, 16);
    emit_imul_reg_reg(code, REG_RCX, REG_RAX);
    emit_add_reg_reg(code, REG_RSI, REG_RCX);

    int not_greater;
    if (has_cmp) {
        emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 0);
        emit_push_reg(code, REG_RAX);
        emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 8);
        emit_push_reg(code, REG_RAX);
        emit_load_mem_disp32(code, REG_RAX, REG_RSI, 0);
        emit_push_reg(code, REG_RAX);
        emit_load_mem_disp32(code, REG_RAX, REG_RSI, 8);
        emit_push_reg(code, REG_RAX);
        emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 48);
        emit_push_reg(code, REG_RAX);
        emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 56);
        emit_push_reg(code, REG_RAX);
        codegen_call_closure_value(2);
        emit_pop_reg(code, REG_RAX); // result payload
        emit_pop_reg(code, REG_RBX); // result tag (assumed NUMBER)
        emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
        emit_pxor_xmm_xmm(code, XMM1);
        emit_ucomisd(code, XMM0, XMM1);
        not_greater = emit_jcc_rel32(code, COND_BE);
        // the call clobbered RSI: recompute &data[j - 1]
        emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 40);
        emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 16);
        emit_sub_reg_imm8(code, REG_RCX, 1);
        emit_mov_reg_imm64(code, REG_RAX, 16);
        emit_imul_reg_reg(code, REG_RCX, REG_RAX);
        emit_add_reg_reg(code, REG_RSI, REG_RCX);
    } else {
        emit_load_mem_disp32(code, REG_RBX, REG_RSI, 0); // previous tag
        emit_load_mem_disp32(code, REG_RAX, REG_RSI, 8); // previous payload
        emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 48); // key tag
        emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 56); // key payload
        emit_cmp_reg_imm32(code, REG_RBX, TAG_STRING);
        int a_num = emit_jcc_rel32(code, COND_NE);
        emit_cmp_reg_imm32(code, REG_RDX, TAG_STRING);
        int b_num = emit_jcc_rel32(code, COND_NE);
        codegen_string_order();
        int flags_ready = emit_jmp_rel32(code);
        emit_patch_jump(code, a_num);
        emit_patch_jump(code, b_num);
        emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
        emit_movq_xmm_from_reg(code, XMM1, REG_RCX);
        emit_ucomisd(code, XMM0, XMM1);
        emit_patch_jump(code, flags_ready);
        not_greater = emit_jcc_rel32(code, COND_BE);
        // string_order clobbered RSI: recompute &data[j - 1]
        emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 40);
        emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 16);
        emit_sub_reg_imm8(code, REG_RCX, 1);
        emit_mov_reg_imm64(code, REG_RAX, 16);
        emit_imul_reg_reg(code, REG_RCX, REG_RAX);
        emit_add_reg_reg(code, REG_RSI, REG_RCX);
    }

    // data[j] = data[j - 1]; j--
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 0);
    emit_store_mem_disp32(code, REG_RSI, 16, REG_RAX);
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 8);
    emit_store_mem_disp32(code, REG_RSI, 24, REG_RAX);
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 40);
    emit_sub_reg_imm8(code, REG_RCX, 1);
    emit_store_mem_disp32(code, REG_RBP, base + 40, REG_RCX);
    emit_jmp_back(code, inner);

    // data[j] = key; i++
    emit_patch_jump(code, place);
    emit_patch_jump(code, not_greater);
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 40);
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 16);
    emit_mov_reg_imm64(code, REG_RAX, 16);
    emit_imul_reg_reg(code, REG_RCX, REG_RAX);
    emit_add_reg_reg(code, REG_RSI, REG_RCX);
    emit_load_mem_disp32(code, REG_RBX, REG_RBP, base + 48);
    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 56);
    emit_store_mem_disp32(code, REG_RSI, 0, REG_RBX);
    emit_store_mem_disp32(code, REG_RSI, 8, REG_RAX);
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 32);
    emit_add_reg_imm8(code, REG_RCX, 1);
    emit_store_mem_disp32(code, REG_RBP, base + 32, REG_RCX);
    emit_jmp_back(code, outer);

    emit_patch_jump(code, all_done);
    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 64);
    emit_mov_reg_imm64(code, REG_RBX, TAG_ARRAY);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}
