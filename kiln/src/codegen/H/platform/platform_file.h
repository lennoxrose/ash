#ifndef KILN_PLATFORM_FILE_H
#define KILN_PLATFORM_FILE_H
#include "codegen/H/emit/emit.h"

// Real file I/O, dispatching on kiln_get_target() internally so call
// sites (codegen/C/io/read_file_builtin.c, codegen/C/io/write_file_builtins.c)
// don't need their own target branch. The fd/HANDLE kiln gets back is
// used the same way on both targets: a signed value, negative means
// "open failed" (Linux's -1 and Windows' INVALID_HANDLE_VALUE, which is
// also -1, both fail the exact same `cmp rax,0; jge ok` check already in
// use -- no call-site change needed there).
void platform_emit_open_read(CodeBuf *code);       // in: RDI=nullterm path -> out: RAX=fd/handle
void platform_emit_open_write(CodeBuf *code, int append); // in: RDI=nullterm path -> out: RAX=fd/handle
void platform_emit_close(CodeBuf *code);           // in: RDI=fd/handle
void platform_emit_read_bytes(CodeBuf *code);      // in: RDI=fd/handle, RSI=buf, RDX=len
void platform_emit_write_bytes(CodeBuf *code);     // in: RDI=fd/handle, RSI=buf, RDX=len
// Replaces the lseek(END)+lseek(SET) dance read_file_builtin.c used on
// Linux: Windows has a direct GetFileSize call, no seek round-trip
// needed. Files over 4GB aren't supported (matches this project's
// existing "don't over-engineer for what can't practically happen" scope
// limit already documented elsewhere for file I/O).
void platform_emit_file_size(CodeBuf *code);       // in: RDI=fd/handle -> out: RAX=size

// Filesystem operations. All three return RAX = 0 on success, nonzero on failure,
// on both targets.
void platform_emit_rename(CodeBuf *code);          // in: RDI=nullterm from, RSI=nullterm to
void platform_emit_delete(CodeBuf *code);          // in: RDI=nullterm path
void platform_emit_mkdir(CodeBuf *code);           // in: RDI=nullterm path

#endif
