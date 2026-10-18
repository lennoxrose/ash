#include <stdio.h>
#include <stdlib.h>
#include "vm/H/vm.h"
#include "vm/H/state.h"
#include "vm/H/gc.h"
#include "compiler/H/compiler.h"

void vm_run(Chunk *main_chunk) {
    stack_top = stack;
    frame_count = 1;
    frames[0].chunk = main_chunk;
    frames[0].ip = main_chunk->code;
    frames[0].slots = stack;
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
    GC_INHIBIT_ENTER();
    vm_execute(caller_frame_count);
    GC_INHIBIT_LEAVE();
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
    GC_INHIBIT_ENTER();
    vm_execute(caller_frame_count);
    GC_INHIBIT_LEAVE();
    return *--stack_top;
}

VMValue vm_call_callable_sync(VMValue callable, VMValue *args, int argc) {
    if (callable.type == VM_FUNCTION) return vm_call_function_sync((int)callable.number, args, argc);
    if (callable.type == VM_CLOSURE) return vm_call_closure_sync(callable.closure, args, argc);
    fprintf(stderr, "not callable\n");
    exit(1);
}
