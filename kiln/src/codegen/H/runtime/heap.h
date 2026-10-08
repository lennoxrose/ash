#ifndef KILN_HEAP_H
#define KILN_HEAP_H
#include "codegen/H/emit/emit.h"

// A bump allocator, no free (matches ashvm's own lack of GC -- neither
// engine reclaims memory during a run, only at process exit). Grows in
// fixed-size chunks instead of one fixed-size arena: Linux extends the
// break with `brk` (one argument, the new break address, in RDI -- mmap
// needs six, three of which, R10/R8/R9, kiln has deliberately never
// needed and would require extending emit.c's register handling to
// reach); Windows reserves a large virtual range up front (no physical
// memory committed) and commits one more chunk at a time via
// VirtualAlloc. Either way the arena is one contiguous region for its
// whole lifetime, so a pointer handed out by an earlier allocation is
// never invalidated by a later growth step.

// Emitted once, at the very start of the generated program (before the
// top-level statement loop): grabs the first chunk and records its start
// as the heap bump pointer at KILN_GLOBALS_ADDR (see elf/H/elf_writer.h),
// plus the committed limit the alloc routine grows from (see heap.c).
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

// Arena-style checkpoint/reset for short-lived temporaries (built for
// Pyre's own lowering, e.g. a tight inner loop's scratch buffers that
// never outlive one iteration): rewinding the bump pointer reclaims
// everything allocated since the checkpoint without any OS call, at the
// cost of being a *rewind*, not a real free -- it is only sound when the
// caller (the code generator emitting the checkpoint/reset pair, not
// these functions) can prove nothing allocated in between is reachable
// after the reset. Nothing here checks that proof; a reset after a
// pointer escaped (stored outside the region, returned, captured by a
// closure, handed to a function that might retain it) corrupts that
// pointer's memory the moment something else allocates over it. Not
// wired into general `each`/`during` loop bodies for exactly that reason
// -- arbitrary Ash code can make a value escape in ways kiln's single-
// pass compiler has no general way to rule out (that is goals.md item
// 1's full ownership/borrow story, not this).
//
// Two instructions each, used directly at the call site rather than
// through heap_emit_alloc's shared-routine-plus-CALL machinery -- that
// indirection earns its keep at ~27 call sites, not at the handful a
// narrowly-scoped optimization like this one will ever have.

// No input; returns the current heap_ptr (the checkpoint value) in RAX,
// clobbers RSI.
void heap_emit_checkpoint(CodeBuf *code);
// Checkpoint value (as returned by heap_emit_checkpoint) in RDI; no
// return value, clobbers RSI. Sets heap_ptr back to it -- heap_limit is
// untouched, since rewinding never needs to give committed pages back.
void heap_emit_reset(CodeBuf *code);

#endif
