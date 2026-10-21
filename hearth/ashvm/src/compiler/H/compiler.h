#ifndef ASH_VM_COMPILER_H
#define ASH_VM_COMPILER_H
#include "vm/H/chunk.h"

// OP_CALL addresses a function with one byte; the first 256 functions use it, the rest OP_CALL_WIDE (16 bits),
// so the hot path of a small program is unchanged. 4096 is a measured ceiling: the table is static, and at 16384
// entries (2 MB of bss) every start-up of every program got ~5% slower.
#define MAX_VM_FUNCS 4096
#define MAX_VM_LOCALS 128

typedef struct {
    char name[64];
    int arity;
    int defined; // 0 while only forward-declared by the prescan (see compiler/C/prescan.c)
    Chunk chunk;
} VMFunction;

extern VMFunction vm_functions[MAX_VM_FUNCS];
extern int vm_function_count;

// Compiles the internal IR unit produced by hearth/front (jdriver). Exits with a diagnostic on error.
Chunk *compile(const char *source);

#endif
