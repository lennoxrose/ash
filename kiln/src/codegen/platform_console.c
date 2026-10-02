#include "codegen/platform_console.h"
#include "codegen/win_call.h"
#include "codegen/win_file_call.h"
#include "parser/vars.h"
#include "target.h"

#define STD_INPUT_HANDLE ((uint64_t)(int64_t)-10)
#define STD_OUTPUT_HANDLE ((uint64_t)(int64_t)-11)
#define STD_ERROR_HANDLE ((uint64_t)(int64_t)-12)

// GetStdHandle(nStdHandle) -- result in RAX. RSI/RDI (whatever the caller
// is using to hold buf/len across this) survive untouched: both are
// non-volatile (callee-saved) in the Windows x64 ABI, unlike RDX/R8-R11.
static void get_std_handle(CodeBuf *code, uint64_t which) {
    win_call_begin(code, 0);
    win_call_arg_imm64(code, 0, which);
    win_call_import(code, PE_IMPORT_GET_STD_HANDLE);
    win_call_end(code);
}

static void windows_write(CodeBuf *code, uint64_t handle) {
    emit_mov_reg_reg(code, REG_RDI, REG_RDX); // len survives get_std_handle in RDI (non-volatile)
    get_std_handle(code, handle);
    win_file_call(code, PE_IMPORT_WRITE_FILE);
}

void platform_emit_write_stdout(CodeBuf *code) {
    if (kiln_get_target() == KILN_TARGET_WINDOWS) { windows_write(code, STD_OUTPUT_HANDLE); return; }
    emit_mov_reg_imm64(code, REG_RDI, 1);
    emit_mov_reg_imm64(code, REG_RAX, 1); // syscall: write
    emit_syscall(code);
}

void platform_emit_write_stderr(CodeBuf *code) {
    if (kiln_get_target() == KILN_TARGET_WINDOWS) { windows_write(code, STD_ERROR_HANDLE); return; }
    emit_mov_reg_imm64(code, REG_RDI, 2);
    emit_mov_reg_imm64(code, REG_RAX, 1); // syscall: write
    emit_syscall(code);
}

void platform_emit_read_stdin_byte(CodeBuf *code) {
    if (kiln_get_target() == KILN_TARGET_WINDOWS) {
        get_std_handle(code, STD_INPUT_HANDLE);
        platform_emit_read_stdin_byte_h(code);
        return;
    }
    emit_mov_reg_imm64(code, REG_RDI, 0);
    emit_mov_reg_imm64(code, REG_RAX, 0); // syscall: read
    emit_syscall(code);
}

void platform_emit_get_stdin_handle(CodeBuf *code) {
    if (kiln_get_target() == KILN_TARGET_WINDOWS) get_std_handle(code, STD_INPUT_HANDLE);
    // Linux has no per-call handle to fetch (fd 0 is a fixed constant) --
    // callers that cache "the handle" across a loop just get an unused
    // RAX on that target, harmless.
}

void platform_emit_read_stdin_byte_h(CodeBuf *code) {
    if (kiln_get_target() == KILN_TARGET_WINDOWS) {
        emit_mov_reg_reg(code, REG_RDI, REG_RDX); // len (always 1) survives in RDI
        win_file_call(code, PE_IMPORT_READ_FILE); // RAX already holds the handle
        // ReadFile doesn't report "bytes read" in RAX (it's a BOOL success
        // flag there) -- read it back out of the out-param scratch this
        // call just wrote, so the caller's "did we get a byte" check
        // still works the same way it does on the Linux read(2) path.
        emit_load_mem_disp32(code, REG_RAX, REG_RBP, win_outparam_offset());
        return;
    }
    emit_mov_reg_imm64(code, REG_RDI, 0);
    emit_mov_reg_imm64(code, REG_RAX, 0); // syscall: read
    emit_syscall(code);
}
