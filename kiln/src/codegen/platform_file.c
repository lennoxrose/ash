#include "codegen/platform_file.h"
#include "codegen/win_call.h"
#include "codegen/win_file_call.h"
#include "target.h"

#define GENERIC_READ 0x80000000u
#define GENERIC_WRITE 0x40000000u
#define FILE_APPEND_DATA 0x00000004u // access-right-only open => every write auto-goes to EOF, no seek needed
#define FILE_SHARE_READ 1u
#define OPEN_EXISTING 3u
#define OPEN_ALWAYS 4u
#define CREATE_ALWAYS 2u
#define FILE_ATTRIBUTE_NORMAL 0x80u

static void create_file(CodeBuf *code, uint64_t access, uint64_t disposition) {
    win_call_begin(code, 3);
    win_call_arg_reg(code, 0, REG_RDI); // path
    win_call_arg_imm64(code, 1, access);
    win_call_arg_imm64(code, 2, FILE_SHARE_READ);
    win_call_arg_imm64(code, 3, 0); // lpSecurityAttributes = NULL
    win_call_stack_arg_imm64(code, 0, disposition);
    win_call_stack_arg_imm64(code, 1, FILE_ATTRIBUTE_NORMAL);
    win_call_stack_arg_imm64(code, 2, 0); // hTemplateFile = NULL
    win_call_import(code, PE_IMPORT_CREATE_FILE_A);
    win_call_end(code);
}

void platform_emit_open_read(CodeBuf *code) {
    if (kiln_get_target() == KILN_TARGET_WINDOWS) { create_file(code, GENERIC_READ, OPEN_EXISTING); return; }
    emit_mov_reg_imm64(code, REG_RSI, 0); // O_RDONLY
    emit_mov_reg_imm64(code, REG_RDX, 0);
    emit_mov_reg_imm64(code, REG_RAX, 2); // syscall: open
    emit_syscall(code);
}

void platform_emit_open_write(CodeBuf *code, int append) {
    if (kiln_get_target() == KILN_TARGET_WINDOWS) {
        create_file(code, append ? FILE_APPEND_DATA : GENERIC_WRITE, append ? OPEN_ALWAYS : CREATE_ALWAYS);
        return;
    }
    // O_WRONLY|O_CREAT|O_TRUNC = 577; O_WRONLY|O_CREAT|O_APPEND = 1089
    // (octal flag bits from Linux's fcntl.h).
    emit_mov_reg_imm64(code, REG_RSI, (uint64_t)(append ? 1089 : 577));
    emit_mov_reg_imm64(code, REG_RDX, 420); // mode 0644
    emit_mov_reg_imm64(code, REG_RAX, 2); // syscall: open
    emit_syscall(code);
}

void platform_emit_close(CodeBuf *code) {
    if (kiln_get_target() == KILN_TARGET_WINDOWS) {
        win_call_begin(code, 0);
        win_call_arg_reg(code, 0, REG_RDI);
        win_call_import(code, PE_IMPORT_CLOSE_HANDLE);
        win_call_end(code);
        return;
    }
    emit_mov_reg_imm64(code, REG_RAX, 3); // syscall: close
    emit_syscall(code);
}

void platform_emit_read_bytes(CodeBuf *code) {
    if (kiln_get_target() == KILN_TARGET_WINDOWS) {
        emit_mov_reg_reg(code, REG_RAX, REG_RDI); // handle
        emit_mov_reg_reg(code, REG_RDI, REG_RDX); // len
        win_file_call(code, PE_IMPORT_READ_FILE); // RAX=handle, RSI=buf (unchanged), RDI=len
        return;
    }
    emit_mov_reg_imm64(code, REG_RAX, 0); // syscall: read
    emit_syscall(code);
}

void platform_emit_write_bytes(CodeBuf *code) {
    if (kiln_get_target() == KILN_TARGET_WINDOWS) {
        emit_mov_reg_reg(code, REG_RAX, REG_RDI); // handle
        emit_mov_reg_reg(code, REG_RDI, REG_RDX); // len
        win_file_call(code, PE_IMPORT_WRITE_FILE);
        return;
    }
    emit_mov_reg_imm64(code, REG_RAX, 1); // syscall: write
    emit_syscall(code);
}

// Contract on both targets: returns the size in RAX, leaves the file's
// read position untouched. GetFileSize gets this for free; Linux's lseek
// doesn't have a "size" query, only "seek and report new position", so
// the Linux path pairs a seek-to-end with a seek back to the start it
// found the fd at (offset 0, since this is only ever called right after
// platform_emit_open_read).
void platform_emit_file_size(CodeBuf *code) {
    if (kiln_get_target() == KILN_TARGET_WINDOWS) {
        win_call_begin(code, 0);
        win_call_arg_reg(code, 0, REG_RDI);
        win_call_arg_imm64(code, 1, 0); // lpFileSizeHigh = NULL (files under 4GB only)
        win_call_import(code, PE_IMPORT_GET_FILE_SIZE);
        win_call_end(code);
        return; // GetFileSize's DWORD result lands in EAX, which -- per
                 // standard x86-64 semantics and every real-world Windows
                 // DLL's own codegen -- already zero-extends into RAX.
    }
    emit_mov_reg_imm64(code, REG_RSI, 0);
    emit_mov_reg_imm64(code, REG_RDX, 2); // SEEK_END
    emit_mov_reg_imm64(code, REG_RAX, 8); // syscall: lseek
    emit_syscall(code);
    emit_push_reg(code, REG_RAX); // save size -- the rewind below clobbers RAX
    emit_mov_reg_imm64(code, REG_RSI, 0);
    emit_mov_reg_imm64(code, REG_RDX, 0); // SEEK_SET
    emit_mov_reg_imm64(code, REG_RAX, 8);
    emit_syscall(code); // rewind (syscall preserves RDI, the fd survives for the caller's next use)
    emit_pop_reg(code, REG_RAX);
}
