#ifndef KILN_ARRAYS_H
#define KILN_ARRAYS_H

// primary()'s '[' handling: parses `[e0, e1, ...]`, allocates a heap
// block sized for exactly that many elements (see codegen/value.h for
// the layout), and pushes (tag=TAG_ARRAY, payload=<block address>).
void codegen_array_literal(void);

// Assumes index already pushed (top) and array already pushed (below
// it), both (tag, payload) pairs -- postfix()'s `arr[i]` read. Pops both,
// bounds-checks, pushes the element.
void codegen_array_index_read(void);

// Assumes value pushed (top), index pushed (middle), array pushed
// (bottom) -- parser.c's `arr[i] = v;` write. Pops all three,
// bounds-checks, stores, pushes nothing (statement context).
void codegen_array_index_store(void);

// Both assume their arguments already pushed left-to-right (matching
// the normal call-argument convention) -- codegen/builtins.c's dispatch
// calls these after parsing push(arr, val) / len(arr)'s arguments.

// push(array, value): appends, growing (a fresh, bigger heap block plus
// a copy -- the bump allocator can't grow in place) if out of capacity.
// Pushes the array back (matching ashvm's push(), which returns args[0]).
void codegen_builtin_push(void);

// len(array): pushes the element count as a NUMBER.
void codegen_builtin_len(void);

#endif
