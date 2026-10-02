#ifndef KILN_STRING_ALLOC_H
#define KILN_STRING_ALLOC_H
#include "codegen/emit.h"

// Emitted once, at program start (see parser.c's compile_program) --
// must run after heap_emit_startup (this calls heap_emit_alloc) and
// before any call to string_alloc_emit_prefixed below.
void string_alloc_emit_startup(CodeBuf *code);

// Plan B: emits ONLY the prefixed-alloc routine's body -- used by the
// standalone "compile the runtime" driver that builds libkilnrt.so.
void string_alloc_emit_prefixed_routine_only(CodeBuf *code);
int string_alloc_prefixed_routine_offset(void);

// Allocates a heap block for a new TAG_STRING value's content: input
// RDX=content length, output RAX=block address with the length prefix
// already written at [RAX+0] (the payload is RAX+8, same layout
// codegen/strings.c's literals use). Clobbers RSI, RDI (via
// codegen/heap.c's heap_emit_alloc) -- RDX itself survives untouched.
// Shared by every milestone-10 string builtin that produces a new string
// (upper/lower/trim/substring/join/replace) so the "+8 for the length
// prefix" arithmetic exists in exactly one place. Plan B, phase B1: a
// real CALL to one shared copy rather than re-emitting inline at each of
// its ~13 call sites.
void string_alloc_emit_prefixed(CodeBuf *code);

#endif
