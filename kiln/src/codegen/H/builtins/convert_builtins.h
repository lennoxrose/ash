#ifndef KILN_CONVERT_BUILTINS_H
#define KILN_CONVERT_BUILTINS_H

// Assumes its one argument already pushed -- codegen/C/builtins/builtins.c's dispatch
// calls these after parsing str(x)/num(x)'s argument.

// str(number): formats into a heap-allocated, length-prefixed string
// (same layout as any other TAG_STRING value) -- the exact same digit
// algorithm codegen/C/runtime/print_int.c uses (sign, integer part backward, up to
// 6 fraction digits with round-half-up and trailing-zero trim), just
// targeting a heap block instead of a direct stdout write. Kept as its
// own self-contained copy rather than sharing code with print_int.c's
// already-tested routine, to not risk regressing it.
void codegen_builtin_str(void);

// num(string): a hand-written strtod-equivalent (no libc in this
// freestanding binary) -- skips leading spaces/tabs, an optional
// sign, integer digits, an optional '.' and fraction digits. Stops at
// the first character it can't consume; a string with no valid numeric
// prefix at all yields 0.0, matching strtod's own "no conversion"
// behavior.
void codegen_builtin_num(void);

#endif
