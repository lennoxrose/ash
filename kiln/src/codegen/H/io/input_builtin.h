#ifndef KILN_INPUT_BUILTIN_H
#define KILN_INPUT_BUILTIN_H

// input(): no arguments -- codegen/C/builtins/builtins.c's dispatch just expects the
// closing ')' before calling this.
void codegen_builtin_input(void);

#endif
