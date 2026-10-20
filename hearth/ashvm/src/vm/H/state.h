#ifndef ASH_VM_STATE_H
#define ASH_VM_STATE_H
#include <setjmp.h>
#include "vm/H/chunk.h"

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
    int gc_inhibit_saved; // GC builds only: nested-callback depth to restore when this handler fires
} TryHandler;

#define MAX_VM_GLOBALS 256
extern VMValue vm_globals[MAX_VM_GLOBALS]; // module-level variables of imported files
extern VMValue stack[STACK_MAX];
extern VMValue *stack_top;
extern CallFrame frames[FRAMES_MAX];
extern int frame_count;

extern TryHandler try_handlers[MAX_TRY_DEPTH];
extern int try_depth;
extern char error_message[512];

// Set by vm_raise_value() right before it longjmps, consumed (and cleared)
// by do_TRY_PUSH's recovery branch in dispatch.c -- lets a caught `raise`
// hand back the exact VMValue the program raised instead of being forced
// through the fixed-size error_message C-string buffer every other runtime
// error uses.
extern VMValue raised_value;
extern int has_raised_value;


// Source position of the instruction currently executing, captured once per
// DISPATCH() (the single point every instruction passes through) so
// vm_runtime_error() can point diagnostics at the right spot without every
// opcode handler having to thread it through.
extern const char *current_pos;
extern int current_poslen;

// The call stack at the latest raise that no user `attempt` could catch (try_depth == 1: only the program's own
// top-level handler is active), innermost frame first. An exception that a user `catch` or `finally` sees first
// gets its stack when it is raised again at the top level (a rethrow), so the code that handles exceptions
// ordinarily never pays for a capture. Read by the trace_* builtins to print the stack of an
// uncaught exception; capturing it costs a copy of the frames and only happens when an exception is raised.
extern int trace_count;
extern unsigned trace_gen;                 // bumped by every capture, so the builtins resolve a stack once
extern Chunk *trace_chunk[FRAMES_MAX];
extern uint8_t *trace_ip[FRAMES_MAX];
void vm_capture_trace(void);

void type_error(const char *context) __attribute__((noreturn));
double require_num(VMValue v, const char *context);
int truthy(VMValue v, const char *context);

void vm_execute(int stop_at_frame_count);

#endif
