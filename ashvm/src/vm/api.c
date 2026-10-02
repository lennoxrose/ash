#include <stdio.h>
#include <stdlib.h>
#include "vm/vm.h"
#include "vm/state.h"
#include "compiler/compiler.h"

void vm_set_repl_recovery(jmp_buf *jb) {
    repl_recovery_jmp = jb;
}

void vm_repl_checkpoint(void) {
    repl_saved_frame_count = frame_count;
    repl_saved_stack_top = stack_top;
}

void vm_run(Chunk *main_chunk) {
    stack_top = stack;
    frame_count = 1;
    frames[0].chunk = main_chunk;
    frames[0].ip = main_chunk->code;
    frames[0].slots = stack;
    vm_execute(0);
}

// Runs bytecode starting at start_offset within the SAME persistent chunk,
// used by the REPL: on the first call, resets the stack/frames like vm_run;
// on later calls, keeps the existing stack/frames (so variables persist) and
// just resumes execution at the newly-appended bytecode's start.
void vm_run_from(Chunk *chunk, int start_offset, int is_first_call) {
    if (is_first_call) {
        stack_top = stack;
        frame_count = 1;
        frames[0].chunk = chunk;
        frames[0].slots = stack;
    }
    frames[0].ip = chunk->code + start_offset;
    vm_execute(0);
}

VMValue vm_call_function_sync(int func_idx, VMValue *args, int argc) {
    for (int i = 0; i < argc; i++) *stack_top++ = args[i];
    int caller_frame_count = frame_count;
    VMFunction *fn = &vm_functions[func_idx];
    CallFrame *volatile newf = &frames[frame_count++];
    newf->chunk = &fn->chunk;
    newf->ip = fn->chunk.code;
    newf->slots = stack_top - argc;
    vm_execute(caller_frame_count);
    return *--stack_top;
}

VMValue vm_call_closure_sync(VMClosureObj *c, VMValue *args, int argc) {
    for (int i = 0; i < argc; i++) *stack_top++ = args[i];
    for (int i = 0; i < c->capture_count; i++) *stack_top++ = c->captured[i];
    int caller_frame_count = frame_count;
    VMFunction *fn = &vm_functions[c->function_index];
    CallFrame *volatile newf = &frames[frame_count++];
    newf->chunk = &fn->chunk;
    newf->ip = fn->chunk.code;
    newf->slots = stack_top - argc - c->capture_count;
    vm_execute(caller_frame_count);
    return *--stack_top;
}

VMValue vm_call_callable_sync(VMValue callable, VMValue *args, int argc) {
    if (callable.type == VM_FUNCTION) return vm_call_function_sync((int)callable.number, args, argc);
    if (callable.type == VM_CLOSURE) return vm_call_closure_sync(callable.closure, args, argc);
    fprintf(stderr, "not callable\n");
    exit(1);
}
