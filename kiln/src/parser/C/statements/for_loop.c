#include "parser/H/statements/for_loop.h"
#include "parser/H/core/parser.h"
#include "parser/H/core/parser_internal.h"
#include "parser/H/declarations/vars.h"
#include "parser/H/statements/loop_stack.h"
#include "codegen/H/expressions/expr.h"
#include "codegen/H/emit/emit.h"

static int for_nesting_depth = 0;

void for_depth_save(int *out) { *out = for_nesting_depth; }
void for_depth_reset(void) { for_nesting_depth = 0; }
void for_depth_restore(int saved) { for_nesting_depth = saved; }

// for (x in array) { block }
//
// The array expression is evaluated once, up front; x is bound to each
// element in turn via the same var-slot mechanism `let` uses, so it's a
// perfectly ordinary variable inside the body (readable, reassignable,
// though reassigning it doesn't affect iteration -- the next iteration
// overwrites it again from the array, matching a plain for-each's usual
// semantics). data_ptr/count/i live in this nesting level's fixed
// RBP-relative slot (see vars.h's for_level_offset) so they survive
// whatever the body's own codegen does, including calls.
void for_statement(void) {
    VarBlockScope saved_scope = vars_scope_begin();
    advance_token(); // consume 'for'
    expect(TOKEN_LPAREN, "expected '(' after 'for'");
    expect(TOKEN_IDENTIFIER, "expected loop variable name");
    const char *var_name = previous.start;
    int var_len = previous.length;
    expect(TOKEN_IN, "expected 'in' after loop variable");
    codegen_expression(); // pushes the array (tag, payload)
    expect(TOKEN_RPAREN, "expected ')' after loop expression");

    if (for_nesting_depth >= MAX_KILN_FOR_DEPTH) parse_error("too many nested for loops");
    int32_t level = for_level_offset(for_nesting_depth);
    for_nesting_depth++;

    emit_pop_reg(code, REG_RAX); // array payload (object address)
    emit_pop_reg(code, REG_RBX); // array tag (ignored, assumed ARRAY)
    emit_load_mem_disp32(code, REG_RCX, REG_RAX, 8);  // count
    emit_load_mem_disp32(code, REG_RDX, REG_RAX, 16); // data_ptr
    emit_store_mem_disp32(code, REG_RBP, level + 0, REG_RDX);
    emit_store_mem_disp32(code, REG_RBP, level + 8, REG_RCX);
    emit_mov_reg_imm64(code, REG_RAX, 0);
    emit_store_mem_disp32(code, REG_RBP, level + 16, REG_RAX); // i = 0

    int slot = resolve_var_in_current_scope(var_name, var_len);
    if (slot == -1) slot = declare_var(var_name, var_len);

    int loop_start = code->count;
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, level + 16); // i
    emit_load_mem_disp32(code, REG_RDX, REG_RBP, level + 8);  // count
    emit_cmp_reg_reg(code, REG_RCX, REG_RDX);
    int loop_exit = emit_jcc_rel32(code, COND_GE);

    emit_load_mem_disp32(code, REG_RSI, REG_RBP, level + 0); // data_ptr
    emit_mov_reg_reg(code, REG_RAX, REG_RCX);
    emit_mov_reg_imm64(code, REG_RDX, 16);
    emit_imul_reg_reg(code, REG_RAX, REG_RDX);
    emit_add_reg_reg(code, REG_RSI, REG_RAX); // element address

    emit_load_mem_disp32(code, REG_RBX, REG_RSI, 0); // element tag
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 8); // element payload
    emit_store_mem_disp32(code, REG_RBP, var_slot_tag_offset(slot), REG_RBX);
    emit_store_mem_disp32(code, REG_RBP, var_slot_payload_offset(slot), REG_RAX);

    loop_push();
    block();
    loop_patch_continues(); // continue lands here, right before i++

    emit_load_mem_disp32(code, REG_RCX, REG_RBP, level + 16);
    emit_add_reg_imm8(code, REG_RCX, 1);
    emit_store_mem_disp32(code, REG_RBP, level + 16, REG_RCX); // i++
    emit_jmp_back(code, loop_start);
    emit_patch_jump(code, loop_exit);
    loop_pop_and_patch_breaks(); // break lands here, after the loop

    for_nesting_depth--;
    vars_scope_end(saved_scope);
}
