#ifndef KILN_MATRIX_MUL_H
#define KILN_MATRIX_MUL_H

// matrix_mul(a, b): only available under --link=shared (ideas/assigned.md
// -- a --link=static binary stays the hand-rolled, zero-dependency ELF/PE
// kiln otherwise always produces; this builtin's whole implementation is
// a call through the GOT into libpyre.so, which only exists under
// --link=shared at all). codegen/C/builtins/builtins.c's dispatch calls
// this after parsing matrix_mul(...)'s arguments, same convention as
// every other builtin in that file -- a and b already pushed
// left-to-right. Rejects (compile error, not a runtime one -- link mode
// is known at compile time) with a clear message under --link=static.
void codegen_builtin_matrix_mul(void);

#endif
