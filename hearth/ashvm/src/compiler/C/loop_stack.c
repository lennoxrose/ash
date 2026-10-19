#include <stdlib.h>
#include "compiler/H/loop_stack.h"
#include "compiler/H/state.h"
#include "diagnostics/H/diagnostics.h"

#define MAX_LOOP_NESTING 16
#define MAX_LOOP_JUMPS 64

typedef struct {
    int stop_jumps[MAX_LOOP_JUMPS];
    int stop_count;
    int next_jumps[MAX_LOOP_JUMPS];
    int next_count;
    int attempt_base; // attempt_depth when the loop began
    int local_base;   // local_count when the body began (stop must pop anything declared past it)
    int next_leaves;  // a `next` was compiled while locals past local_base were live (see loop_next_leaves_locals)
} LoopContext;

static LoopContext loop_stack[MAX_LOOP_NESTING];
static int loop_depth = 0;
static int loop_floor = 0;    // loops below this index belong to an enclosing function body (see loop_scope_reset)
static int attempt_depth = 0; // attempt blocks currently open in this function body

void loop_push(void) {
    if (loop_depth >= MAX_LOOP_NESTING) {
        diagnostics_report("error", "too many nested loops", current.start, current.length);
        exit(1);
    }
    loop_stack[loop_depth].stop_count = 0;
    loop_stack[loop_depth].next_count = 0;
    loop_stack[loop_depth].attempt_base = attempt_depth;
    loop_stack[loop_depth].local_base = local_count;
    loop_stack[loop_depth].next_leaves = 0;
    loop_depth++;
}

void attempt_enter(void) { attempt_depth++; }
void attempt_leave(void) { attempt_depth--; }

static void emit_try_pops(int count) {
    for (int i = 0; i < count; i++) emit_op(OP_TRY_POP);
}

void emit_attempt_unwind_all(void) { emit_try_pops(attempt_depth); }

// stop/next leave every attempt block opened since the innermost loop began.
void emit_attempt_unwind_to_loop(void) {
    if (loop_depth > loop_floor) emit_try_pops(attempt_depth - loop_stack[loop_depth - 1].attempt_base);
}

// `stop` jumps past the loop's own cleanup, so the locals the body declared before the stop
// (in the body or in blocks nested inside it) are still on the stack: pop them first, or every
// later local in the function lands in the wrong slot. (`next` lands before the cleanup pops.)
void emit_loop_local_pops(void) {
    if (loop_depth <= loop_floor) return;
    int extra = local_count - loop_stack[loop_depth - 1].local_base;
    for (int i = 0; i < extra; i++) emit_op(OP_POP);
}

void loop_record_stop(Token keyword, int patch_offset) {
    if (loop_depth <= loop_floor) {
        diagnostics_report("error", "'stop' outside a loop", keyword.start, keyword.length);
        exit(1);
    }
    LoopContext *ctx = &loop_stack[loop_depth - 1];
    if (ctx->stop_count >= MAX_LOOP_JUMPS) {
        diagnostics_report("error", "too many stop statements in one loop", keyword.start, keyword.length);
        exit(1);
    }
    ctx->stop_jumps[ctx->stop_count++] = patch_offset;
}

void loop_record_next(Token keyword, int patch_offset) {
    if (loop_depth <= loop_floor) {
        diagnostics_report("error", "'next' outside a loop", keyword.start, keyword.length);
        exit(1);
    }
    LoopContext *ctx = &loop_stack[loop_depth - 1];
    if (ctx->next_count >= MAX_LOOP_JUMPS) {
        diagnostics_report("error", "too many next statements in one loop", keyword.start, keyword.length);
        exit(1);
    }
    ctx->next_jumps[ctx->next_count++] = patch_offset;
    if (local_count > ctx->local_base) ctx->next_leaves = 1;
}

// Did a `next` of the innermost loop leave from inside a nested block that had declared locals? Those are still on
// the stack where `next` lands, and the loop's own cleanup has to cut them off even when the body declared none.
int loop_next_leaves_locals(void) { return loop_depth > 0 && loop_stack[loop_depth - 1].next_leaves; }

void loop_patch_nexts(void) {
    LoopContext *ctx = &loop_stack[loop_depth - 1];
    for (int i = 0; i < ctx->next_count; i++) patch_jump(ctx->next_jumps[i]);
}

void loop_pop_and_patch_stops(void) {
    LoopContext *ctx = &loop_stack[loop_depth - 1];
    for (int i = 0; i < ctx->stop_count; i++) patch_jump(ctx->stop_jumps[i]);
    loop_depth--;
}

void loop_scope_save(LoopScope *out) { out->loops = loop_depth; out->attempts = attempt_depth; out->floor = loop_floor; }
// The body compiled next has no loop of its own yet, but the contexts of the loops it is written inside still hold
// the jumps waiting to be patched: its loops stack above them instead of reusing their slots.
void loop_scope_reset(void) { loop_floor = loop_depth; attempt_depth = 0; }
void loop_scope_restore(LoopScope saved) { loop_depth = saved.loops; attempt_depth = saved.attempts; loop_floor = saved.floor; }
