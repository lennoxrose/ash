#include <stddef.h>
#include "vm/H/state.h"

VMValue stack[STACK_MAX];
VMValue vm_globals[MAX_VM_GLOBALS];
VMValue *stack_top;
CallFrame frames[FRAMES_MAX];
int frame_count;

TryHandler try_handlers[MAX_TRY_DEPTH];
int try_depth = 0;
char error_message[512];

VMValue raised_value;
int has_raised_value = 0;


int trace_count = 0;
unsigned trace_gen = 1;
Chunk *trace_chunk[FRAMES_MAX];
uint8_t *trace_ip[FRAMES_MAX];

const char *current_pos = NULL;
int current_poslen = 0;
