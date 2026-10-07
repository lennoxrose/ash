#ifndef ASH_VM_STATE_H
#define ASH_VM_STATE_H
#include <setjmp.h>
#include "vm/chunk.h"

// Internal VM state shared between vm_dispatch.c, vm_errors.c, and vm_api.c
// -- not part of the public API (vm.h). Mirrors the same
// declare-storage-once, share-via-header pattern ashc used to use for its
// parser state (see rundown.md).

#define STACK_MAX 65536
#define FRAMES_MAX 256
#define MAX_TRY_DEPTH 32

typedef struct {
    Chunk *chunk;
    uint8_t *ip;
    VMValue *slots;
} CallFrame;

typedef struct {
    jmp_buf jmp;
    int saved_frame_count;
    VMValue *saved_stack_top;
    CallFrame *target_frame;
    uint8_t *target_ip;
} TryHandler;

extern VMValue stack[STACK_MAX];
extern VMValue *stack_top;
extern CallFrame frames[FRAMES_MAX];
extern int frame_count;

extern TryHandler try_handlers[MAX_TRY_DEPTH];
extern int try_depth;
extern char error_message[512];

extern jmp_buf *repl_recovery_jmp;
extern int repl_saved_frame_count;
extern VMValue *repl_saved_stack_top;

// Source position of the instruction currently executing, captured once per
// DISPATCH() (the single point every instruction passes through) so
// vm_runtime_error() can point diagnostics at the right spot without every
// opcode handler having to thread it through.
extern const char *current_pos;
extern int current_poslen;

void type_error(const char *context) __attribute__((noreturn));
double require_num(VMValue v, const char *context);
int truthy(VMValue v, const char *context);

void vm_execute(int stop_at_frame_count);

#endif
