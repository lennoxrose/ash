#include "parser/H/statements/loop_stack.h"
#include "parser/H/core/parser.h"

#define MAX_LOOP_NESTING 16
#define MAX_LOOP_JUMPS 64

typedef struct {
    int stop_jumps[MAX_LOOP_JUMPS];
    int stop_count;
    int next_jumps[MAX_LOOP_JUMPS];
    int next_count;
} LoopContext;

static LoopContext loop_stack[MAX_LOOP_NESTING];
static int loop_depth = 0;

void loop_push(void) {
    if (loop_depth >= MAX_LOOP_NESTING) parse_error("too many nested loops");
    loop_stack[loop_depth].stop_count = 0;
    loop_stack[loop_depth].next_count = 0;
    loop_depth++;
}

void loop_record_stop(int patch_offset) {
    if (loop_depth == 0) parse_error("'stop' outside a loop");
    LoopContext *ctx = &loop_stack[loop_depth - 1];
    if (ctx->stop_count >= MAX_LOOP_JUMPS) parse_error("too many stop statements in one loop");
    ctx->stop_jumps[ctx->stop_count++] = patch_offset;
}

void loop_record_next(int patch_offset) {
    if (loop_depth == 0) parse_error("'next' outside a loop");
    LoopContext *ctx = &loop_stack[loop_depth - 1];
    if (ctx->next_count >= MAX_LOOP_JUMPS) parse_error("too many next statements in one loop");
    ctx->next_jumps[ctx->next_count++] = patch_offset;
}

void loop_patch_nexts(void) {
    LoopContext *ctx = &loop_stack[loop_depth - 1];
    for (int i = 0; i < ctx->next_count; i++) emit_patch_jump(code, ctx->next_jumps[i]);
}

void loop_pop_and_patch_stops(void) {
    LoopContext *ctx = &loop_stack[loop_depth - 1];
    for (int i = 0; i < ctx->stop_count; i++) emit_patch_jump(code, ctx->stop_jumps[i]);
    loop_depth--;
}

void loop_depth_save(int *out) { *out = loop_depth; }
void loop_depth_reset(void) { loop_depth = 0; }
void loop_depth_restore(int saved) { loop_depth = saved; }
