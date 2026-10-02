#ifndef ASH_VM_VM_H
#define ASH_VM_VM_H
#include <setjmp.h>
#include "vm/chunk.h"

void vm_run(Chunk *main_chunk);
void vm_run_from(Chunk *chunk, int start_offset, int is_first_call);
void vm_set_repl_recovery(jmp_buf *jb);
void vm_repl_checkpoint(void);

VMValue vm_call_function_sync(int func_idx, VMValue *args, int argc);
VMValue vm_call_closure_sync(VMClosureObj *c, VMValue *args, int argc);
VMValue vm_call_callable_sync(VMValue callable, VMValue *args, int argc);
void vm_runtime_error(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));

#endif
