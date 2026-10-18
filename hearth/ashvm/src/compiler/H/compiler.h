#ifndef ASH_VM_COMPILER_H
#define ASH_VM_COMPILER_H
#include "vm/H/chunk.h"

// OP_CALL addresses a function with ONE byte, so 256 is the real limit; a larger table silently
// truncated indices and called the wrong function.
#define MAX_VM_FUNCS 256
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
