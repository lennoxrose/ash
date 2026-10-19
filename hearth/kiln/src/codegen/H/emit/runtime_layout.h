#ifndef KILN_RUNTIME_LAYOUT_H
#define KILN_RUNTIME_LAYOUT_H

// Second globals slot, right after heap_ptr: the current end of committed
// heap memory (the Linux brk value / the Windows committed boundary). See
// codegen/C/runtime/heap.c -- the bump allocator grows into fresh OS pages
// once heap_ptr would cross this line, instead of the old fixed 16MB cap.
#define KILN_HEAP_LIMIT_SIZE 8

// OS-agnostic sizes for kiln's own hand-rolled attempt/handle handler stack
// (see codegen/C/runtime/errors.c's raise routine, parser/C/statements/attempt_handle.c) -- the byte
// layout is a kiln-internal invention, not tied to ELF or PE, so both
// elf/H/elf_writer.h and pe/H/pe_writer.h include this rather than each
// re-declaring the same numbers.
#define MAX_KILN_TRY_DEPTH 16
#define KILN_TRY_HANDLER_SIZE 32
#define KILN_TRY_HANDLERS_SIZE (MAX_KILN_TRY_DEPTH * KILN_TRY_HANDLER_SIZE)
#define KILN_TRY_DEPTH_SIZE 8


// Heads of the free lists of the static builds' allocator (see codegen/C/runtime/heap.c): one 8-byte slot per
// 16-byte size class, 16..512 bytes, slot 0 unused. Zero in the file, so every list starts empty.
#define KILN_FREELIST_SIZE (33 * 8)

// Stack-trace area of the static builds (codegen/C/runtime/trace.c): the call stack captured when a raise reaches
// the program's top-level handler. [0] number of return addresses, [8] the rbp of the top-level program frame
// (where the walk of the rbp chain stops), then the addresses, innermost first.
#define KILN_TRACE_MAX 32
#define KILN_TRACE_SIZE (16 + 8 * KILN_TRACE_MAX)

// Module-level state: top-level `local`s of an imported file whose value is
// computed at import time live in a fixed table of (tag, payload) slots in the
// globals region (see parser/C/declarations/module_state.c), because a called
// function's own frame can't reach the importing frame.
#define KILN_MODSTATE_SLOTS 128
#define KILN_MODSTATE_SIZE (16 * KILN_MODSTATE_SLOTS)

#endif
