#ifndef KILN_ELF_DYNAMIC_CALL_H
#define KILN_ELF_DYNAMIC_CALL_H
#include "elf/elf_dynamic.h"

// Calls `which` through its GOT slot (filled in by ld.so before this
// program's own code ever runs) -- a single `call [addr]` touching no
// register, so it's safe to insert in front of ANY of the runtime
// routines regardless of which registers that specific routine's own
// argument convention uses (heap_alloc: RDI/RAX, print_top: RAX/RBX,
// bytes_copy: RDI/RBX/RDX, ...). Used for RAISE too even though the
// static-link build reaches it via a JMP, not a CALL (it never returns):
// the extra return address CALL pushes is simply abandoned, unreachable
// dead stack space either way, since nothing after a raise site ever
// falls through -- harmless, and not worth a second code path here.
void elf_dynamic_call(CodeBuf *code, RuntimeImport which);

#endif
