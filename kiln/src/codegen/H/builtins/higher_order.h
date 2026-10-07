#ifndef KILN_HIGHER_ORDER_H
#define KILN_HIGHER_ORDER_H

// Each parses its own arguments (array, fn, and for reduce, an initial
// value) starting right after the already-consumed '(' -- codegen/
// builtins.c's dispatch calls these directly, matching push()/len()'s
// convention of self-parsing rather than the caller pre-evaluating args.

// map(array, fn): pushes a new array, each element replaced by fn(element).
void codegen_builtin_map(void);

// filter(array, fn): pushes a new array containing only the elements for
// which fn(element) was truthy (a nonzero NUMBER).
void codegen_builtin_filter(void);

// reduce(array, fn, initial): pushes fn(...fn(fn(initial, e0), e1)..., eN).
void codegen_builtin_reduce(void);

#endif
