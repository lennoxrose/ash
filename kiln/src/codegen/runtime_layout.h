#ifndef KILN_RUNTIME_LAYOUT_H
#define KILN_RUNTIME_LAYOUT_H

// OS-agnostic sizes for kiln's own hand-rolled try/catch handler stack
// (see codegen/errors.c's raise routine, parser/try_catch.c) -- the byte
// layout is a kiln-internal invention, not tied to ELF or PE, so both
// elf/elf_writer.h and pe/pe_writer.h include this rather than each
// re-declaring the same numbers.
#define MAX_KILN_TRY_DEPTH 16
#define KILN_TRY_HANDLER_SIZE 32
#define KILN_TRY_HANDLERS_SIZE (MAX_KILN_TRY_DEPTH * KILN_TRY_HANDLER_SIZE)
#define KILN_TRY_DEPTH_SIZE 8

#endif
