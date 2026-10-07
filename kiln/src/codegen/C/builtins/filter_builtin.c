#include "codegen/higher_order.h"
#include "codegen/expr.h"
#include "codegen/closures.h"
#include "codegen/emit.h"
#include "codegen/emit_sse.h"
#include "codegen/value.h"
#include "codegen/heap.h"
#include "parser/parser.h"
#include "parser/vars.h"

// Scratch layout: [0]=fn_tag [8]=fn_payload [16]=data_ptr [24]=count
//   [32]=out_data_ptr [40]=i [48]=out_count
//
// The output block is sized to the INPUT count (an upper bound on how
// many survive the filter) -- simpler than a second pass or a grow loop,
// and leaves a perfectly ordinary array afterward (capacity=input count,
// count=out_count<=capacity, same shape push() already knows how to grow
// from).
void codegen_builtin_filter(void) {
    codegen_expression(); // array
    expect(TOKEN_COMMA, "expected ',' after array argument");
    codegen_expression(); // fn
    expect(TOKEN_RPAREN, "expected ')' after arguments");

    emit_pop_reg(code, REG_RAX); // fn payload
    emit_pop_reg(code, REG_RBX); // fn tag
    emit_pop_reg(code, REG_RSI); // array object
    emit_pop_reg(code, REG_RCX); // array tag (ignored)

    int32_t base = higher_order_scratch_offset();
    emit_store_mem_disp32(code, REG_RBP, base + 0, REG_RBX);
    emit_store_mem_disp32(code, REG_RBP, base + 8, REG_RAX);

    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 8);  // count
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, 16); // data_ptr
    emit_store_mem_disp32(code, REG_RBP, base + 16, REG_RDX);
    emit_store_mem_disp32(code, REG_RBP, base + 24, REG_RAX);

    emit_mov_reg_reg(code, REG_RDI, REG_RAX);
    emit_mov_reg_imm64(code, REG_RBX, 16);
    emit_imul_reg_reg(code, REG_RDI, REG_RBX);
    emit_cmp_reg_imm32(code, REG_RDI, 0);
    int nonzero = emit_jcc_rel32(code, COND_NE);
    emit_mov_reg_imm64(code, REG_RDI, 1);
    emit_patch_jump(code, nonzero);
    heap_emit_alloc(code); // RAX = out block
    emit_store_mem_disp32(code, REG_RBP, base + 32, REG_RAX);

    emit_mov_reg_imm64(code, REG_RCX, 0);
    emit_store_mem_disp32(code, REG_RBP, base + 40, REG_RCX); // i = 0
    emit_store_mem_disp32(code, REG_RBP, base + 48, REG_RCX); // out_count = 0

    int loop_start = code->count;
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 40);
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 24);
    emit_cmp_reg_reg(code, REG_RCX, REG_RDX);
    int loop_exit = emit_jcc_rel32(code, COND_GE);

    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 16); // data_ptr
    emit_mov_reg_reg(code, REG_RAX, REG_RCX);
    emit_mov_reg_imm64(code, REG_RDX, 16);
    emit_imul_reg_reg(code, REG_RAX, REG_RDX);
    emit_add_reg_reg(code, REG_RSI, REG_RAX); // element address

    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 0); // fn_tag
    emit_push_reg(code, REG_RAX);
    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 8); // fn_payload
    emit_push_reg(code, REG_RAX);
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 0); // element tag
    emit_push_reg(code, REG_RAX);
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 8); // element payload
    emit_push_reg(code, REG_RAX);
    codegen_call_closure_value(1);

    emit_pop_reg(code, REG_RAX); // result payload
    emit_pop_reg(code, REG_RBX); // result tag (ignored, assumed NUMBER)
    emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
    emit_pxor_xmm_xmm(code, XMM1);
    emit_ucomisd(code, XMM0, XMM1);
    int skip_keep = emit_jcc_rel32(code, COND_E); // falsy (0.0) -> don't keep

    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 40); // i (still valid)
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 16); // data_ptr (recompute -- the call clobbered RSI)
    emit_mov_reg_reg(code, REG_RAX, REG_RCX);
    emit_mov_reg_imm64(code, REG_RDX, 16);
    emit_imul_reg_reg(code, REG_RAX, REG_RDX);
    emit_add_reg_reg(code, REG_RSI, REG_RAX); // element address again

    emit_load_mem_disp32(code, REG_RDI, REG_RBP, base + 48); // out_count
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 32); // out_data_ptr
    emit_mov_reg_reg(code, REG_RAX, REG_RDI);
    emit_mov_reg_imm64(code, REG_RCX, 16);
    emit_imul_reg_reg(code, REG_RAX, REG_RCX);
    emit_add_reg_reg(code, REG_RDX, REG_RAX); // out slot address

    emit_load_mem_disp32(code, REG_RCX, REG_RSI, 0); emit_store_mem_disp32(code, REG_RDX, 0, REG_RCX);
    emit_load_mem_disp32(code, REG_RCX, REG_RSI, 8); emit_store_mem_disp32(code, REG_RDX, 8, REG_RCX);

    emit_add_reg_imm8(code, REG_RDI, 1);
    emit_store_mem_disp32(code, REG_RBP, base + 48, REG_RDI); // out_count++
    emit_patch_jump(code, skip_keep);

    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 40);
    emit_add_reg_imm8(code, REG_RCX, 1);
    emit_store_mem_disp32(code, REG_RBP, base + 40, REG_RCX); // i++
    emit_jmp_back(code, loop_start);
    emit_patch_jump(code, loop_exit);

    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 24); // capacity = input count
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 48); // count = out_count
    emit_mov_reg_imm64(code, REG_RDI, ARRAY_OBJECT_SIZE);
    heap_emit_alloc(code); // RAX = array object
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 32); // out_data_ptr (reload)
    emit_store_mem_disp32(code, REG_RAX, 0, REG_RCX);
    emit_store_mem_disp32(code, REG_RAX, 8, REG_RDX);
    emit_store_mem_disp32(code, REG_RAX, 16, REG_RSI);

    emit_mov_reg_imm64(code, REG_RBX, TAG_ARRAY);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}
