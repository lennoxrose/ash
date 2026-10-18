#ifndef KILN_ARGV_BUILTIN_H
#define KILN_ARGV_BUILTIN_H

// argv(): no arguments -- codegen/C/builtins/builtins.c's dispatch just expects the
// closing ')' before calling this. Pushes an array of strings, one per
// process argument (argv[0] is the executable's own path, same
// convention as C's main(argc, argv)).
void codegen_builtin_argv(void);

#endif
