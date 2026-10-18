#include "parser/H/core/parser.h"
#include "parser/H/core/parser_internal.h"
#include "parser/H/declarations/vars.h"
#include "parser/H/statements/loop_stack.h"
#include "codegen/H/emit/emit.h"
#include "codegen/H/runtime/errors.h"
#include "codegen/H/emit/layout.h"
#include "codegen/H/emit/runtime_layout.h"

// attempt { A } handle (e) { B }
//
// A handler stack living in the fixed globals region (see
// elf/H/elf_writer.h) holds, per active attempt, the (rsp, rbp) to restore
// and the (target address, error-variable rbp-offset) to resume at --
// kiln's own setjmp/longjmp equivalent, hand-rolled since this is a
// freestanding binary with no libc to call into (see
// codegen/C/runtime/errors.c's raise routine for the runtime half of this).
//
// The handle variable's slot has to be reserved BEFORE compiling A: its
// rbp-relative offset must be embedded into the handler struct at
// attempt-entry, which runs before A's body -- and therefore before `e`'s
// real name, which only appears after A -- is even parsed. A placeholder
// slot is declared up front and retroactively renamed once `handle (e)`
// is actually seen. attempt/handle is inline (like given/during), not a
// new call frame, so this works the same way given/during's own bodies
// share the enclosing function's variable table.
void attempt_emit_handler_pop(void) {
    emit_mov_reg_imm64(code, REG_RDX, kiln_try_depth_addr());
    emit_load_mem_disp32(code, REG_RCX, REG_RDX, 0);
    emit_dec_reg(code, REG_RCX);
    emit_store_mem_disp32(code, REG_RDX, 0, REG_RCX);
}

void attempt_statement(void) {
    VarBlockScope saved_scope = vars_scope_begin();
    advance_token(); // consume 'attempt'

    int handle_slot = declare_var("$handle", 7);

    // --- push handler (bounds-checked -- overflowing here would
    // silently corrupt the code segment right after the handler table) ---
    emit_mov_reg_imm64(code, REG_RAX, kiln_try_depth_addr());
    emit_load_mem_disp32(code, REG_RAX, REG_RAX, 0);
    emit_cmp_reg_imm32(code, REG_RAX, MAX_KILN_TRY_DEPTH);
    int overflow = emit_jcc_rel32(code, COND_GE);

    emit_mov_reg_imm64(code, REG_RCX, KILN_TRY_HANDLER_SIZE);
    emit_imul_reg_reg(code, REG_RAX, REG_RCX); // RAX = index * handler size
    emit_mov_reg_imm64(code, REG_RDI, kiln_try_handlers_addr());
    emit_add_reg_reg(code, REG_RDI, REG_RAX); // RDI = this handler's slot address

    emit_store_mem_disp32(code, REG_RDI, 0, REG_RSP);
    emit_store_mem_disp32(code, REG_RDI, 8, REG_RBP);
    int patch_offset = emit_mov_reg_imm64_patchable(code, REG_RCX); // handle target, patched once known
    emit_store_mem_disp32(code, REG_RDI, 16, REG_RCX);
    emit_mov_reg_imm64(code, REG_RCX, (uint64_t)(int64_t)var_slot_tag_offset(handle_slot));
    emit_store_mem_disp32(code, REG_RDI, 24, REG_RCX);

    emit_mov_reg_imm64(code, REG_RAX, kiln_try_depth_addr());
    emit_load_mem_disp32(code, REG_RCX, REG_RAX, 0);
    emit_add_reg_imm8(code, REG_RCX, 1);
    emit_store_mem_disp32(code, REG_RAX, 0, REG_RCX);

    int skip_overflow_die = emit_jmp_rel32(code);
    emit_patch_jump(code, overflow);
    errors_emit_fatal(code, "runtime error: too many nested try blocks");
    emit_patch_jump(code, skip_overflow_die);

    attempt_enter();
    block(); // compiles A
    attempt_leave();

    // Normal completion (no error was raised): pop the handler -- past
    // this point, an error inside the enclosing scope should NOT be
    // caught by this attempt anymore.
    attempt_emit_handler_pop();

    int skip_handle = emit_jmp_rel32(code);
    int handle_code_offset = code->count;
    uint64_t handle_addr = kiln_code_base() + (uint64_t)handle_code_offset;
    emit_patch_imm64(code, patch_offset, handle_addr);

    expect(TOKEN_HANDLE, "expected 'handle' after attempt block");
    expect(TOKEN_LPAREN, "expected '(' after 'handle'");
    expect(TOKEN_IDENTIFIER, "expected error variable name");
    rename_var(handle_slot, previous.start, previous.length);
    expect(TOKEN_RPAREN, "expected ')' after handle variable");

    block(); // compiles B -- `e` already declared at handle_slot above

    emit_patch_jump(code, skip_handle);
    vars_scope_end(saved_scope);
}
