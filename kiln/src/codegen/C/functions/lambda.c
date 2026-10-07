#include <string.h>
#include "codegen/closures.h"
#include "codegen/closures_internal.h"
#include "codegen/emit.h"
#include "codegen/value.h"
#include "codegen/expr.h"
#include "codegen/layout.h"
#include "parser/parser.h"
#include "parser/parser_internal.h"
#include "parser/vars.h"
#include "parser/functions.h"
#include "parser/for_loop.h"
#include "parser/loop_stack.h"

// `fn (params) { body }` as an expression. The closure-object creation
// code (which runs in the OUTER frame, snapshotting captures by value)
// is emitted first, then a jump-over, then the body -- same shape as
// parser_control.c's fn_statement, generalized with a capture region
// ahead of the parameter region. The object's code address isn't known
// until after the body compiles, so it's written via a patchable imm64
// (see emit_mov_reg_imm64_patchable) and patched once the body's end is
// reached.
void codegen_lambda_expr(void) {
    advance_token(); // consume 'fn'

    VarScope outer_vars;
    vars_save(&outer_vars);
    int capture_count = outer_vars.count;

    closures_alloc_object(capture_count); // RBX = object address
    int patch_offset = emit_mov_reg_imm64_patchable(code, REG_RCX);
    emit_store_mem_disp32(code, REG_RBX, 0, REG_RCX); // [obj+0] = code addr, patched below

    for (int i = 0; i < capture_count; i++) {
        emit_load_mem_disp32(code, REG_RAX, REG_RBP, var_slot_tag_offset(i));
        emit_store_mem_disp32(code, REG_RBX, 16 + 16 * i, REG_RAX);
        emit_load_mem_disp32(code, REG_RAX, REG_RBP, var_slot_payload_offset(i));
        emit_store_mem_disp32(code, REG_RBX, 16 + 16 * i + 8, REG_RAX);
    }
    closures_push_value();

    int skip_jump = emit_jmp_rel32(code);
    int lambda_code_offset = code->count;

    vars_clear();
    int outer_for_depth;
    for_depth_save(&outer_for_depth);
    for_depth_reset();
    int outer_loop_depth;
    loop_depth_save(&outer_loop_depth);
    loop_depth_reset();
    for (int i = 0; i < capture_count; i++) {
        declare_var(outer_vars.names[i], (int)strlen(outer_vars.names[i]));
    }

    expect(TOKEN_LPAREN, "expected '(' after 'fn'");
    char param_names[MAX_KILN_PARAMS][64];
    int param_lens[MAX_KILN_PARAMS];
    int argc = 0;
    if (current.type != TOKEN_RPAREN) {
        for (;;) {
            expect(TOKEN_IDENTIFIER, "expected parameter name");
            if (argc >= MAX_KILN_PARAMS) parse_error("too many parameters");
            param_lens[argc] = previous.length;
            memcpy(param_names[argc], previous.start, (size_t)previous.length);
            argc++;
            if (current.type != TOKEN_COMMA) break;
            advance_token();
        }
    }
    expect(TOKEN_RPAREN, "expected ')' after parameters");

    emit_push_reg(code, REG_RBP);
    emit_mov_reg_reg(code, REG_RBP, REG_RSP);
    emit_sub_reg_imm32(code, REG_RSP, KILN_FRAME_RESERVE);

    // Captures sit nearest to rbp (pushed last by codegen_indirect_call),
    // args sit further out, past all the captures -- see closures.c's
    // header comment for the full stack shape.
    for (int i = 0; i < capture_count; i++) {
        int base = 16 + 16 * (capture_count - 1 - i);
        emit_load_mem_disp32(code, REG_RAX, REG_RBP, base);
        emit_store_mem_disp32(code, REG_RBP, var_slot_payload_offset(i), REG_RAX);
        emit_load_mem_disp32(code, REG_RBX, REG_RBP, base + 8);
        emit_store_mem_disp32(code, REG_RBP, var_slot_tag_offset(i), REG_RBX);
    }
    for (int i = 0; i < argc; i++) {
        int slot = declare_var(param_names[i], param_lens[i]);
        int base = 16 + 16 * capture_count + 16 * (argc - 1 - i);
        emit_load_mem_disp32(code, REG_RAX, REG_RBP, base);
        emit_store_mem_disp32(code, REG_RBP, var_slot_payload_offset(slot), REG_RAX);
        emit_load_mem_disp32(code, REG_RBX, REG_RBP, base + 8);
        emit_store_mem_disp32(code, REG_RBP, var_slot_tag_offset(slot), REG_RBX);
    }

    block();

    emit_mov_reg_imm64(code, REG_RAX, 0); // implicit `return 0;`
    emit_mov_reg_imm64(code, REG_RBX, TAG_NUMBER);
    emit_mov_reg_reg(code, REG_RSP, REG_RBP);
    emit_pop_reg(code, REG_RBP);
    emit_ret(code);

    vars_restore(&outer_vars);
    for_depth_restore(outer_for_depth);
    loop_depth_restore(outer_loop_depth);
    emit_patch_jump(code, skip_jump);

    uint64_t addr = kiln_code_base() + (uint64_t)lambda_code_offset;
    emit_patch_imm64(code, patch_offset, addr);
}
