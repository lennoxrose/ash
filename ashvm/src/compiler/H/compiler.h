#ifndef ASH_VM_COMPILER_H
#define ASH_VM_COMPILER_H
#include "vm/chunk.h"

#define MAX_VM_FUNCS 64
#define MAX_VM_LOCALS 128

typedef struct {
    char name[64];
    int arity;
    Chunk chunk;
} VMFunction;

extern VMFunction vm_functions[MAX_VM_FUNCS];
extern int vm_function_count;

Chunk *compile(const char *source, const char *source_path);
Chunk *compile_repl_line(const char *source, Chunk *target_chunk, int *out_start_offset);

#endif
