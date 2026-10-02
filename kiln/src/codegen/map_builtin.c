#include "codegen/higher_order.h"
#include "codegen/expr.h"
#include "codegen/closures.h"
#include "codegen/emit.h"
#include "codegen/value.h"
#include "codegen/heap.h"
#include "parser/parser.h"
#include "parser/vars.h"

// Scratch layout (see vars.h's higher_order_scratch_offset -- everything
// needed after codegen_call_closure_value must live here, never in a
// register, since the call clobbers all of them):
//   [0]=fn_tag [8]=fn_payload [16]=data_ptr [24]=count
//   [32]=out_data_ptr [40]=i
void codegen_builtin_map(void) {
    codegen_expression(); // array
    expect(TOKEN_COMMA, "expected ',' after array argument");
    codegen_expression(); // fn
    expect(TOKEN_RPAREN, "expected ')' after arguments");

    emit_pop_reg(code, REG_RAX); // fn payload
    emit_pop_reg(code, REG_RBX); // fn tag
    emit_pop_reg(code, REG_RSI); // array object
    emit_pop_reg(code, REG_RCX); // array tag (ignored, assumed ARRAY)

    int32_t base = higher_order_scratch_offset();
    emit_store_mem_disp32(code, REG_RBP, base + 0, REG_RBX);
    emit_store_mem_disp32(code, REG_RBP, base + 8, REG_RAX);

    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 8);  // count
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, 16); // data_ptr
    emit_store_mem_disp32(code, REG_RBP, base + 16, REG_RDX);
    emit_store_mem_disp32(code, REG_RBP, base + 24, REG_RAX);

    // output elements block, sized to match (count could be 0 at runtime)
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
    emit_pop_reg(code, REG_RBX); // result tag

    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 40); // i
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 32); // out_data_ptr
    emit_mov_reg_reg(code, REG_RDX, REG_RCX);
    emit_mov_reg_imm64(code, REG_RDI, 16);
    emit_imul_reg_reg(code, REG_RDX, REG_RDI);
    emit_add_reg_reg(code, REG_RSI, REG_RDX);
    emit_store_mem_disp32(code, REG_RSI, 0, REG_RBX);
    emit_store_mem_disp32(code, REG_RSI, 8, REG_RAX);

    emit_add_reg_imm8(code, REG_RCX, 1);
    emit_store_mem_disp32(code, REG_RBP, base + 40, REG_RCX);
    emit_jmp_back(code, loop_start);
    emit_patch_jump(code, loop_exit);

    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 24); // count
    emit_mov_reg_imm64(code, REG_RDI, ARRAY_OBJECT_SIZE);
    heap_emit_alloc(code); // RAX = array object
    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 32); // out_data_ptr (reload -- alloc clobbers RSI)
    emit_store_mem_disp32(code, REG_RAX, 0, REG_RCX);  // capacity
    emit_store_mem_disp32(code, REG_RAX, 8, REG_RCX);  // count
    emit_store_mem_disp32(code, REG_RAX, 16, REG_RSI); // data_ptr

    emit_mov_reg_imm64(code, REG_RBX, TAG_ARRAY);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}
