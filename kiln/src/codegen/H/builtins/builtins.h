#ifndef KILN_BUILTINS_H
#define KILN_BUILTINS_H

// Called from primary()'s function-call handling right after the '('
// has been consumed. If `name` (length `len`) is a known builtin, parses
// its arguments itself (through the closing ')'), emits its codegen, and
// returns 1. Returns 0 without consuming anything otherwise, so the
// caller falls through to a user-defined function lookup.
int codegen_try_builtin_call(const char *name, int len);

#endif
