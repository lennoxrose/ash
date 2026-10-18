#ifndef KILN_ELF_SO_WRITER_H
#define KILN_ELF_SO_WRITER_H

// Compiles the runtime (heap/bytes/string_alloc/print/errors-raise) into
// a fresh, position-independent CodeBuf and writes it as a real ET_DYN
// ELF64 shared object at `path` -- one exported symbol per
// codegen/H/emit/runtime_exports.def entry, loadable by the system's real
// dynamic linker (validated structure, see elf/H/elf_dynamic.h's header
// comment). Returns 0 on success.
int elf_write_shared_runtime(const char *path);

#endif
