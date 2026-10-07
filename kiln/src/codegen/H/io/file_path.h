#ifndef KILN_FILE_PATH_H
#define KILN_FILE_PATH_H
#include "codegen/emit.h"

// Kiln strings are length-prefixed, not null-terminated -- but the raw
// `open` syscall (like libc's) requires a null-terminated path. Input:
// RSI=string payload, RDX=length. Output: RAX=address of a fresh
// heap-allocated null-terminated copy. Clobbers RBX, RCX, RDI (and RSI,
// RDX transiently, restored from scratch internally).
void filepath_emit_nullterm(CodeBuf *code);

#endif
