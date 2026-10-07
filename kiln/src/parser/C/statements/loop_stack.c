#include "parser/H/statements/loop_stack.h"
#include "parser/H/core/parser.h"

#define MAX_LOOP_NESTING 16
#define MAX_LOOP_JUMPS 64

typedef struct {
    int break_jumps[MAX_LOOP_JUMPS];
    int break_count;
    int continue_jumps[MAX_LOOP_JUMPS];
    int continue_count;
} LoopContext;

static LoopContext loop_stack[MAX_LOOP_NESTING];
static int loop_depth = 0;

void loop_push(void) {
    if (loop_depth >= MAX_LOOP_NESTING) parse_error("too many nested loops");
    loop_stack[loop_depth].break_count = 0;
    loop_stack[loop_depth].continue_count = 0;
    loop_depth++;
}

void loop_record_break(int patch_offset) {
    if (loop_depth == 0) parse_error("'break' outside a loop");
    LoopContext *ctx = &loop_stack[loop_depth - 1];
    if (ctx->break_count >= MAX_LOOP_JUMPS) parse_error("too many break statements in one loop");
    ctx->break_jumps[ctx->break_count++] = patch_offset;
}

void loop_record_continue(int patch_offset) {
    if (loop_depth == 0) parse_error("'continue' outside a loop");
    LoopContext *ctx = &loop_stack[loop_depth - 1];
    if (ctx->continue_count >= MAX_LOOP_JUMPS) parse_error("too many continue statements in one loop");
    ctx->continue_jumps[ctx->continue_count++] = patch_offset;
}

void loop_patch_continues(void) {
    LoopContext *ctx = &loop_stack[loop_depth - 1];
    for (int i = 0; i < ctx->continue_count; i++) emit_patch_jump(code, ctx->continue_jumps[i]);
}

void loop_pop_and_patch_breaks(void) {
    LoopContext *ctx = &loop_stack[loop_depth - 1];
    for (int i = 0; i < ctx->break_count; i++) emit_patch_jump(code, ctx->break_jumps[i]);
    loop_depth--;
}

void loop_depth_save(int *out) { *out = loop_depth; }
void loop_depth_reset(void) { loop_depth = 0; }
void loop_depth_restore(int saved) { loop_depth = saved; }
