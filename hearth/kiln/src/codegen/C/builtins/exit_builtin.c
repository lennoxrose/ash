#include "codegen/H/builtins/exit_builtin.h"
#include "codegen/H/expressions/expr.h"
#include "codegen/H/emit/emit.h"
#include "codegen/H/emit/emit_sse.h"
#include "codegen/H/platform/win_call.h"
#include "app/H/target.h"
#include "parser/H/core/parser.h"

// exit(code): ends the process with that status. Output is written
// unbuffered, so there is nothing to flush first.
void codegen_builtin_exit(void) {
    codegen_expression();
    expect(TOKEN_RPAREN, "expected ')' after argument");
    emit_pop_reg(code, REG_RAX); // payload
    emit_pop_reg(code, REG_RBX); // tag (ignored)
    emit_movq_xmm_from_reg(code, XMM0, REG_RAX);
    emit_cvttsd2si(code, REG_RDI, XMM0);
    if (kiln_get_target() == KILN_TARGET_WINDOWS) {
        win_call_begin(code, 0);
        win_call_arg_reg(code, 0, REG_RDI);
        win_call_import(code, PE_IMPORT_EXIT_PROCESS);
    } else {
        // exit_group (231), not exit (60) -- see runtime/print_int.c's
        // codegen_exit0 for why (the same reasoning applies here).
        emit_mov_reg_imm64(code, REG_RAX, 231);
        emit_syscall(code);
    }
    // never returns; keep the stack-machine model balanced for the caller
    emit_mov_reg_imm64(code, REG_RBX, 5 /* TAG_NIL */);
    emit_push_reg(code, REG_RBX);
    emit_push_reg(code, REG_RBX);
}
