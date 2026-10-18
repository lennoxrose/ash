#include "codegen/H/builtins/higher_order.h"
#include "codegen/H/expressions/expr.h"
#include "codegen/H/functions/closures.h"
#include "codegen/H/emit/emit.h"
#include "parser/H/core/parser.h"
#include "parser/H/declarations/vars.h"

// Scratch layout: [0]=acc_tag [8]=acc_payload [16]=fn_tag [24]=fn_payload
//   [32]=data_ptr [40]=count [48]=i
void codegen_builtin_reduce(void) {
    codegen_expression(); // array
    expect(TOKEN_COMMA, "expected ',' after array argument");
    codegen_expression(); // fn
    expect(TOKEN_COMMA, "expected ',' after fn argument");
    codegen_expression(); // initial
    expect(TOKEN_RPAREN, "expected ')' after arguments");

    int32_t base = higher_order_scratch_offset();

    emit_pop_reg(code, REG_RAX); // initial payload
    emit_pop_reg(code, REG_RBX); // initial tag
    emit_store_mem_disp32(code, REG_RBP, base + 0, REG_RBX);
    emit_store_mem_disp32(code, REG_RBP, base + 8, REG_RAX);

    emit_pop_reg(code, REG_RAX); // fn payload
    emit_pop_reg(code, REG_RBX); // fn tag
    emit_store_mem_disp32(code, REG_RBP, base + 16, REG_RBX);
    emit_store_mem_disp32(code, REG_RBP, base + 24, REG_RAX);

    emit_pop_reg(code, REG_RSI); // array object
    emit_pop_reg(code, REG_RCX); // array tag (ignored)
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 8);  // count
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, 16); // data_ptr
    emit_store_mem_disp32(code, REG_RBP, base + 32, REG_RDX);
    emit_store_mem_disp32(code, REG_RBP, base + 40, REG_RAX);

    emit_mov_reg_imm64(code, REG_RCX, 0);
    emit_store_mem_disp32(code, REG_RBP, base + 48, REG_RCX); // i = 0

    int loop_start = code->count;
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 48);
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, base + 40);
    emit_cmp_reg_reg(code, REG_RCX, REG_RDX);
    int loop_exit = emit_jcc_rel32(code, COND_GE);

    emit_load_mem_disp32(code, REG_RSI, REG_RBP, base + 32); // data_ptr
    emit_mov_reg_reg(code, REG_RAX, REG_RCX);
    emit_mov_reg_imm64(code, REG_RDX, 16);
    emit_imul_reg_reg(code, REG_RAX, REG_RDX);
    emit_add_reg_reg(code, REG_RSI, REG_RAX); // element address

    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 16); // fn_tag
    emit_push_reg(code, REG_RAX);
    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 24); // fn_payload
    emit_push_reg(code, REG_RAX);
    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 0); // acc_tag (arg0)
    emit_push_reg(code, REG_RAX);
    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 8); // acc_payload
    emit_push_reg(code, REG_RAX);
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 0); // element tag (arg1)
    emit_push_reg(code, REG_RAX);
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 8); // element payload
    emit_push_reg(code, REG_RAX);
    codegen_call_closure_value(2);

    emit_pop_reg(code, REG_RAX); // result payload
    emit_pop_reg(code, REG_RBX); // result tag
    emit_store_mem_disp32(code, REG_RBP, base + 0, REG_RBX); // acc = result
    emit_store_mem_disp32(code, REG_RBP, base + 8, REG_RAX);

    emit_load_mem_disp32(code, REG_RCX, REG_RBP, base + 48);
    emit_add_reg_imm8(code, REG_RCX, 1);
    emit_store_mem_disp32(code, REG_RBP, base + 48, REG_RCX); // i++
    emit_jmp_back(code, loop_start);
    emit_patch_jump(code, loop_exit);

    emit_load_mem_disp32(code, REG_RBX, REG_RBP, base + 0); // acc_tag
    emit_load_mem_disp32(code, REG_RAX, REG_RBP, base + 8); // acc_payload
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RAX);
}
