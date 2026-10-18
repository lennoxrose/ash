#ifndef KILN_MEM_BUILTIN_H
#define KILN_MEM_BUILTIN_H

// Memory builtins of the IR (front/src/jown + jlower emit them, System.free lowers to mem_free).
// Both self-parse their arguments, starting right after the already-consumed '('.
//
// mem_release(x): System.free(x) from the program: evaluated and ignored (see mem_builtin.c).
void codegen_builtin_mem_release(void);

// mem_free(x): gives a String back to the allocator (heap.h free lists). Other values, and any program
// that did not enable the free lists (Windows, --link=shared), are evaluated and ignored: the bump heap
// keeps them until exit, like before. Result: none.
void codegen_builtin_mem_free(void);

// mem_free_arr(x): the same for an array made by an array literal / mem_newarr1 (object and element block).
void codegen_builtin_mem_free_arr(void);

// mem_free_obj(x): the same for an object (a plain class instance: map object and entries block).
void codegen_builtin_mem_free_obj(void);

// mem_free_map(x): a HashMap / HashSet (map object, entries block and the prelude's entry arrays).
void codegen_builtin_mem_free_map(void);

// mem_free_elems(x): the fresh Strings inside a String[] / List<String> (see jown); the array itself is not freed.
void codegen_builtin_mem_free_elems(void);

// mem_newarr1(n, d): `new T[n]` for one dimension, filled with d, with exactly n elements of capacity.
void codegen_builtin_mem_newarr1(void);

// mem_cat(a, b, flags): a + b for two Strings, then frees the operands that were temporaries
// (flags bit 0 = a, bit 1 = b), so `"x" + str(i)` does not leave the str(i) text behind. Result: the String.
void codegen_builtin_mem_cat(void);

#endif
