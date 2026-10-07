#ifndef KILN_BYTES_H
#define KILN_BYTES_H
#include "codegen/emit.h"

// Emitted once, at program start (see parser.c's compile_program) --
// must run before any call to bytes_emit_copy below.
void bytes_emit_startup(CodeBuf *code);

// Plan B: emits ONLY the copy routine's body -- used by the standalone
// "compile the runtime" driver that builds libkilnrt.so.
void bytes_emit_copy_routine_only(CodeBuf *code);
int bytes_copy_routine_offset(void);

// RDI=dest, RBX=src, RDX=count (input) -- clobbers RAX,RBX,RCX,RDX,RDI.
// Deliberately doesn't touch RSI, so callers can keep a stable scratch
// pointer there across multiple calls. Shared between codegen/strings.c
// (concatenation) and codegen/arrays.c (growth). Plan B, phase B1: a real
// CALL to one shared copy (emitted by bytes_emit_startup) rather than
// re-emitting this loop inline at each of its ~18 call sites.
void bytes_emit_copy(CodeBuf *code);

#endif
