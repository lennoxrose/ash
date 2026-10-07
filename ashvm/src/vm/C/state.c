#include <stddef.h>
#include "vm/H/state.h"

VMValue stack[STACK_MAX];
VMValue *stack_top;
CallFrame frames[FRAMES_MAX];
int frame_count;

TryHandler try_handlers[MAX_TRY_DEPTH];
int try_depth = 0;
char error_message[512];

VMValue raised_value;
int has_raised_value = 0;

jmp_buf *repl_recovery_jmp = NULL;
int repl_saved_frame_count;
VMValue *repl_saved_stack_top;

const char *current_pos = NULL;
int current_poslen = 0;
