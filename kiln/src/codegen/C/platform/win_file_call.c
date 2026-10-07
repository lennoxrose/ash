#include "codegen/H/platform/win_file_call.h"
#include "codegen/H/platform/win_call.h"
#include "parser/H/declarations/vars.h"

// &ignored_out needs to be a real writable address (Windows doesn't
// accept NULL there for synchronous handles), so it points at a
// throwaway RBP-relative scratch DWORD -- computed via a low register
// first since there's no LEA primitive for the R8/R9 slot directly.
void win_file_call(CodeBuf *code, PeImport which) {
    win_call_begin(code, 1);
    win_call_arg_reg(code, 0, REG_RAX); // handle
    win_call_arg_reg(code, 1, REG_RSI); // buf
    win_call_arg_reg(code, 2, REG_RDI); // len
    emit_mov_reg_reg(code, REG_RAX, REG_RBP);
    emit_add_reg_imm32(code, REG_RAX, win_outparam_offset());
    win_call_arg_reg(code, 3, REG_RAX); // &written/&read (ignored)
    win_call_stack_arg_imm64(code, 0, 0); // lpOverlapped = NULL
    win_call_import(code, which);
    win_call_end(code);
}
