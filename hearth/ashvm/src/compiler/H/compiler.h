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

Chunk *compile(const char *source, const char *source_path);
Chunk *compile_repl_line(const char *source, Chunk *target_chunk, int *out_start_offset);

#endif
