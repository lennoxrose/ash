#ifndef ASH_VM_VM_H
#define ASH_VM_VM_H
#include <setjmp.h>
#include "vm/H/chunk.h"

void vm_run(Chunk *main_chunk);
void vm_run_from(Chunk *chunk, int start_offset, int is_first_call);
void vm_set_repl_recovery(jmp_buf *jb);
void vm_repl_checkpoint(void);

VMValue vm_call_function_sync(int func_idx, VMValue *args, int argc);
VMValue vm_call_closure_sync(VMClosureObj *c, VMValue *args, int argc);
VMValue vm_call_callable_sync(VMValue callable, VMValue *args, int argc);
void vm_runtime_error(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));
// Like vm_runtime_error, but for a RUNTIME string value (`raise expr;`)
// instead of a compile-time format string -- msg must be VM_STR. Unlike
// vm_runtime_error, the caught value is the exact string the program
// raised, not a copy funneled through the fixed-size error_message buffer,
// so an arbitrarily long raised message survives intact.
void vm_raise_value(VMValue msg) __attribute__((noreturn));

#endif
