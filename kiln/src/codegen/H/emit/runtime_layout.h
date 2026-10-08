#ifndef KILN_RUNTIME_LAYOUT_H
#define KILN_RUNTIME_LAYOUT_H

// OS-agnostic sizes for kiln's own hand-rolled attempt/handle handler stack
// (see codegen/C/runtime/errors.c's raise routine, parser/C/statements/attempt_handle.c) -- the byte
// layout is a kiln-internal invention, not tied to ELF or PE, so both
// elf/H/elf_writer.h and pe/H/pe_writer.h include this rather than each
// re-declaring the same numbers.
#define MAX_KILN_TRY_DEPTH 16
#define KILN_TRY_HANDLER_SIZE 32
#define KILN_TRY_HANDLERS_SIZE (MAX_KILN_TRY_DEPTH * KILN_TRY_HANDLER_SIZE)
#define KILN_TRY_DEPTH_SIZE 8


// Module-level state: top-level `local`s of an imported file whose value is
// computed at import time live in a fixed table of (tag, payload) slots in the
// globals region (see parser/C/declarations/module_state.c), because a called
// function's own frame can't reach the importing frame.
#define KILN_MODSTATE_SLOTS 128
#define KILN_MODSTATE_SIZE (16 * KILN_MODSTATE_SLOTS)

#endif
