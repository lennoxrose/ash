#include "codegen/H/functions/closures.h"
#include "codegen/H/functions/closures_internal.h"
#include "codegen/H/emit/emit.h"
#include "codegen/H/expressions/expr.h"
#include "codegen/H/emit/value.h"
#include "codegen/H/runtime/heap.h"
#include "codegen/H/emit/layout.h"
#include "parser/H/core/parser.h"
#include "parser/H/declarations/vars.h"

// Closure-object creation and the two ways a TAG_FUNCTION value gets
// built (a lambda literal's runtime header, and a zero-capture wrapper
// around an already-declared named function). The lambda body's own
// codegen (parsing params, compiling the block) lives in lambda.c --
// split so neither file drifts past ~200 lines.

// Allocates a closure object with room for `capture_count` captures and
// stores capture_count into [obj+8]. Leaves RBX = object address; the
// code address at [obj+0] is the caller's job (patchable for a lambda
// literal, immediate for a named function value -- see below).
void closures_alloc_object(int capture_count) {
    emit_mov_reg_imm64(code, REG_RDI, (uint64_t)(FUNCTION_OBJECT_HEADER_SIZE + 16 * capture_count));
    heap_emit_alloc(code); // RAX = address, clobbers RSI
    emit_mov_reg_reg(code, REG_RBX, REG_RAX);
    emit_mov_reg_imm64(code, REG_RCX, (uint64_t)capture_count);
    emit_store_mem_disp32(code, REG_RBX, 8, REG_RCX);
}

// Pushes (TAG_FUNCTION, RBX) as this expression's result -- RBX must
// already hold the finished closure object's address.
void closures_push_value(void) {
    emit_mov_reg_imm64(code, REG_RCX, TAG_FUNCTION);
    emit_push_reg(code, REG_RCX);
    emit_push_reg(code, REG_RBX);
}

void codegen_named_function_value(KilnFunction *fn) {
    closures_alloc_object(0);
    uint64_t addr = kiln_code_base() + (uint64_t)fn->code_offset;
    emit_mov_reg_imm64(code, REG_RCX, addr);
    emit_store_mem_disp32(code, REG_RBX, 0, REG_RCX);
    closures_push_value();
}

// Assumes a closure's (tag, payload) is already pushed, followed by
// `argc` more (tag, payload) argument pairs already pushed on top of it
// (in order) -- the shared tail of every indirect call, regardless of
// where the closure value and arguments came from. codegen_indirect_call
// below is the "closure from a source-level variable, args from parsed
// syntax" case; codegen/higher_order.c's map/filter/reduce build both
// programmatically instead and call this directly.
//
// Stack shape right before the indirect call (bottom = pushed first):
//   [closure_tag][closure_payload][arg0_tag][arg0_payload]...[argN-1]
//   [capture0_tag][capture0_payload]...[captureM-1]      <- RSP here
// M (capture_count) is only known at runtime (read out of the closure
// object), so it's pushed via a genuine runtime loop -- unlike every
// other loop-shaped codegen in this project, which is always
// compile-time-bounded. Since M must survive the `call` (every GP
// register is freely clobbered by the callee) it's stashed in the fixed
// RBP-relative scratch slot from vars.h for the RSP cleanup afterward.
void codegen_call_closure_value(int argc) {
    emit_mov_reg_reg(code, REG_RAX, REG_RSP);
    emit_load_mem_disp32(code, REG_RSI, REG_RAX, 16 * argc); // closure object address

    emit_load_mem_disp32(code, REG_RCX, REG_RSI, 8); // capture_count
    emit_store_mem_disp32(code, REG_RBP, call_scratch_offset(), REG_RCX);
    emit_load_mem_disp32(code, REG_RDX, REG_RSI, 0); // absolute code address
    emit_add_reg_imm8(code, REG_RSI, 16); // -> first capture

    int loop_start = code->count;
    emit_cmp_reg_imm32(code, REG_RCX, 0);
    int loop_exit = emit_jcc_rel32(code, COND_E);
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 0); // capture tag
    emit_push_reg(code, REG_RAX);
    emit_load_mem_disp32(code, REG_RAX, REG_RSI, 8); // capture payload
    emit_push_reg(code, REG_RAX);
    emit_add_reg_imm8(code, REG_RSI, 16);
    emit_dec_reg(code, REG_RCX);
    emit_jmp_back(code, loop_start);
    emit_patch_jump(code, loop_exit);

    emit_call_indirect(code, REG_RDX);

    // Clean up: closure entry + args (compile-time known) + captures
    // (runtime-known, recovered from the scratch slot -- registers were
    // free to be clobbered by the call, RBP-relative memory wasn't).
    emit_add_reg_imm32(code, REG_RSP, 16 * (1 + argc));
    emit_load_mem_disp32(code, REG_RCX, REG_RBP, call_scratch_offset());
    emit_add_reg_reg(code, REG_RCX, REG_RCX); // *2
    emit_add_reg_reg(code, REG_RCX, REG_RCX); // *4
    emit_add_reg_reg(code, REG_RCX, REG_RCX); // *8
    emit_add_reg_reg(code, REG_RCX, REG_RCX); // *16
    emit_add_reg_reg(code, REG_RSP, REG_RCX);

    emit_push_reg(code, REG_RBX); // return convention: RBX=tag, RAX=payload
    emit_push_reg(code, REG_RAX);
}

// `id` already resolved to variable `slot`; current sits on the first
// argument or ')'. Pushes the closure and parses+pushes the
// source-level arguments, then hands off to the shared tail above.
void codegen_indirect_call(int slot) {
    emit_load_mem_disp32(code, REG_RBX, REG_RBP, var_slot_tag_offset(slot));
    emit_push_reg(code, REG_RBX);
    emit_load_mem_disp32(code, REG_RAX, REG_RBP, var_slot_payload_offset(slot));
    emit_push_reg(code, REG_RAX);

    int argc = 0;
    if (current.type != TOKEN_RPAREN) {
        codegen_expression(); argc++;
        while (current.type == TOKEN_COMMA) { advance_token(); codegen_expression(); argc++; }
    }
    expect(TOKEN_RPAREN, "expected ')' after arguments");

    codegen_call_closure_value(argc);
}
