#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "vm/H/vm.h"
#include "vm/H/state.h"
#include "diagnostics/H/diagnostics.h"

// Every genuinely-runtime error (as opposed to a compile-time/structural one,
// which stays a hard crash since ashvm resolves names at compile time) funnels
// through here. If a try block is active, longjmp back to its handler instead
// of crashing the whole program.
void vm_runtime_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(error_message, sizeof(error_message), fmt, args);
    va_end(args);

    if (try_depth > 0) {
        longjmp(try_handlers[try_depth - 1].jmp, 1);
    } else if (repl_recovery_jmp != NULL) {
        diagnostics_report("error", error_message, current_pos, current_poslen);
        frame_count = repl_saved_frame_count;
        stack_top = repl_saved_stack_top;
        longjmp(*repl_recovery_jmp, 1);
    } else {
        diagnostics_report("error", error_message, current_pos, current_poslen);
        exit(1);
    }
}

void type_error(const char *context) {
    vm_runtime_error("type error: invalid operand type in %s", context);
}

double require_num(VMValue v, const char *context) {
    if (v.type != VM_NUM) type_error(context);
    return v.number;
}

int truthy(VMValue v, const char *context) {
    return require_num(v, context) != 0;
}
