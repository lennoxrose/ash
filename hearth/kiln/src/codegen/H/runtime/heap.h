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

// Free lists (static Linux builds only, and only for programs that free: the driver calls
// heap_enable_free_lists() after lowering when the program contains mem_free / mem_cat). Then the alloc
// string alloc routine (heap_emit_alloc_str) rounds every request to 16 bytes and recycles freed blocks of up
// to 512 bytes through one list per size class (heads in the globals region, kiln_freelist_addr()). The plain
// routine (heap_emit_alloc) is never touched, so arrays, maps and programs that never free cost exactly what
// they always did. Strings are the only thing kiln frees (heap_emit_free_string_rax).
void heap_enable_free_lists(int blocks, int regions); // blocks: the program also frees arrays or objects (mem_free_arr / mem_newarr1 / mem_free_obj); regions: it has loop regions (mem_region_enter), so every free marks slot 0 of the table
int heap_free_lists_on(void);
int heap_free_arrays_on(void);
// RAX = payload of a heap string; puts its block on the free list. Clobbers RAX, RCX, RDX, RSI.
void heap_emit_free_string_rax(CodeBuf *code);
// RAX = an array object (see heap.c for what that requires); frees it and its element block.
void heap_emit_free_array_rax(CodeBuf *code);
// RAX = the rounded 1-byte block of an empty literal; puts it back. Clobbers RAX, RCX, RDX, RSI.
void heap_emit_free_empty_block_rax(CodeBuf *code);
// RAX = a map object (an object of a plain class); frees it and its entries block.
void heap_emit_free_map_rax(CodeBuf *code);
// Allocation of an array or map object or its block: rounded to whole size classes and served from the free
// lists when the program frees arrays, else the plain routine (exactly as before).
void heap_emit_alloc_recyclable(CodeBuf *code);

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
// Same contract, for the two places that make the Strings kiln can free (concatenation, str()); falls back to
// heap_emit_alloc when the free lists are off.
void heap_emit_alloc_str(CodeBuf *code);

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

// The reset of a loop region (jown proved nothing allocated in the loop body outlives its iteration; see
// front/src/jown/C/region.c): checkpoint value in RDI; clobbers RAX, RBX, RCX, RDX, RSI. It also empties the
// free lists, because a block freed inside the region now lies beyond the heap pointer and must never be handed
// out again (blocks freed before the region are given up too: a leak, never an overlap). With the environment
// variable KILN_REGION_POISON set when compiling, the reclaimed bytes are first overwritten with 0xDD, so that
// any use of a value that should have been dead shows up in the tests.
void heap_emit_region_reset(CodeBuf *code);

#endif
