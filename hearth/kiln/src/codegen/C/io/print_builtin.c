#include "codegen/H/io/print_builtin.h"
#include "codegen/H/expressions/expr.h"
#include "codegen/H/emit/emit.h"
#include "codegen/H/emit/value.h"
#include "codegen/H/platform/platform_console.h"
#include "parser/H/core/parser.h"

// print_raw(s): the lowering always hands it a String (System.out.print converts its argument first), laid out as
// [payload-8] = length, [payload..] = bytes, the same block the println routine writes from.
void codegen_builtin_print_raw(void) {
    codegen_expression();
    expect(TOKEN_RPAREN, "expected ')' after argument");
    emit_pop_reg(code, REG_RAX); // payload
    emit_pop_reg(code, REG_RBX); // tag
    emit_load_mem_disp32(code, REG_RDX, REG_RAX, -8); // length
    emit_mov_reg_reg(code, REG_RSI, REG_RAX);          // buf
    platform_emit_write_stdout(code);
    emit_mov_reg_imm64(code, REG_RBX, TAG_NIL);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RBX);
}
