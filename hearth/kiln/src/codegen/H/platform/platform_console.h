#ifndef KILN_PLATFORM_CONSOLE_H
#define KILN_PLATFORM_CONSOLE_H
#include "codegen/H/emit/emit.h"

// Console I/O, dispatching on kiln_get_target() internally so call sites
// (codegen/C/runtime/print_int.c, codegen/C/runtime/errors.c, codegen/C/io/input_builtin.c) don't
// need their own target branch. Linux writes/reads a fixed fd (1/2/0)
// directly via raw syscall; Windows has no such fixed handle -- it has to
// call GetStdHandle first, so each of these costs one extra call there.
//
// platform_emit_write_stdout/stderr: RSI=buf, RDX=len in; no output.
// platform_emit_read_stdin_byte: RSI=1-byte buf, RDX=1 in; RAX=bytes
// actually read (0 or 1) out, matching Linux read(2)'s own return
// convention so input_builtin.c's existing "did we get a byte" check
// (cmp rax,1) works unchanged on both targets.
void platform_emit_write_stdout(CodeBuf *code);
void platform_emit_write_stderr(CodeBuf *code);
void platform_emit_read_stdin_byte(CodeBuf *code);

// Split out of platform_emit_read_stdin_byte for callers that read many
// bytes in a loop (codegen/C/io/input_builtin.c): looking the handle up once
// outside the loop instead of once per byte cuts a multi-byte line's
// worth of GetStdHandle round-trips down to one, which matters on
// Windows -- unlike Linux's fixed fd 0, nothing here is free.
// platform_emit_get_stdin_handle: out RAX=handle (Windows only -- no-op,
// nothing to call, on Linux). platform_emit_read_stdin_byte_h: in
// RAX=handle (as returned above; ignored on Linux), RSI=1-byte buf,
// RDX=1; out RAX=bytes read (0 or 1), same convention as
// platform_emit_read_stdin_byte.
void platform_emit_get_stdin_handle(CodeBuf *code);
void platform_emit_read_stdin_byte_h(CodeBuf *code);

#endif
