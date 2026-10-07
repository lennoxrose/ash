#ifndef KILN_ARGV_BUILTIN_INTERNAL_H
#define KILN_ARGV_BUILTIN_INTERNAL_H
#include "codegen/H/emit/emit.h"

// Windows has no kernel-provided argv array to read (see
// argv_builtin.c's header comment) -- GetCommandLineA() returns the raw
// command-line string instead, so the Windows path needs its own tokenizer
// rather than sharing codegen with the Linux path. Split into its own
// file to keep argv_builtin.c under ~200 lines; not part of the public
// codegen/H/io/argv_builtin.h surface, only argv_builtin.c calls this.
void codegen_builtin_argv_windows(void);

#endif
