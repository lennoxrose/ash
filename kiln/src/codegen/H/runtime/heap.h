#ifndef KILN_HEAP_H
#define KILN_HEAP_H
#include "codegen/emit.h"

// A bump allocator, no free (matches ashvm's own lack of GC -- neither
// engine reclaims memory during a run, only at process exit). Backed by
// `brk` rather than `mmap`: brk takes exactly one argument (the new break
// address, in RDI), while mmap needs six, three of which (R10/R8/R9) kiln
// has deliberately never needed until now and would require extending
// emit.c's register handling to reach (those are all >= 8, needing
// REX.R/X/B bits this project's encoders don't set). brk sidesteps that
// entirely for a one-time, fixed-size arena grab.

// Emitted once, at the very start of the generated program (before the
// top-level statement loop): grabs a 16MB arena and records its start as
// the heap bump pointer at KILN_GLOBALS_ADDR (see elf/elf_writer.h).
void heap_emit_startup(CodeBuf *code);

// Plan B: emits ONLY the alloc routine's body (no arena grab) -- used by
// the standalone "compile the runtime" driver that builds libkilnrt.so,
// where the arena grab still happens in the calling EXECUTABLE instead
// (see heap_emit_startup's own comment on this split).
void heap_emit_alloc_routine_only(CodeBuf *code);
// Offset (within whatever CodeBuf the *_routine_only variant just ran
// against) of the alloc routine's entry point -- used by the "compile
// the runtime" driver to fill in libkilnrt.so's dynsym st_value.
int heap_alloc_routine_offset(void);

// Fixed internal calling convention (not the user-facing one -- this is
// never reachable from ash source, only from other codegen, so it can be
// as simple as it wants): size to allocate in RDI, returns the allocated
// block's address in RAX, clobbers RSI. Plan B, phase B1: this now emits
// a real CALL to one shared copy of the routine (emitted once by
// heap_emit_startup) instead of re-emitting the routine's body inline at
// every one of its ~27 call sites.
void heap_emit_alloc(CodeBuf *code);

#endif
