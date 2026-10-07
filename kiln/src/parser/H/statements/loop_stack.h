#ifndef KILN_LOOP_STACK_H
#define KILN_LOOP_STACK_H

// stop/next need to know which loop they belong to, and loops
// nest -- but unlike each_loop.h's runtime scratch state, stop/next
// need NO runtime state at all: they're pure "emit now, patch later"
// jump bookkeeping (same primitive every other forward jump in this
// project uses), just tracked per-nesting-level at compile time instead
// of resolved immediately.
//
// next can't simply backward-jump to a known offset the way a plain
// loop back-edge does: `next`'s target (the per-iteration advance
// step -- an each-loop's index increment, or nothing for a during loop)
// is only emitted AFTER the body compiles, i.e. after the next
// statement itself. So next jumps are forward-patched too, resolved
// by the loop's own codegen right after block() returns, before
// whatever per-iteration bookkeeping runs next.

// Pushes a fresh loop context (call right before compiling the body).
void loop_push(void);

// Called from parser.c's stop_statement()/next_statement() --
// records a forward jump's patch offset into the innermost loop's list.
// parse_error()s (noreturn) if called outside any loop.
void loop_record_stop(int patch_offset);
void loop_record_next(int patch_offset);

// Patches every recorded next jump to the CURRENT code position --
// call right after block() returns, before emitting the per-iteration
// advance step.
void loop_patch_nexts(void);

// Patches every recorded stop jump to the CURRENT code position, then
// pops the loop context -- call once the loop is fully compiled.
void loop_pop_and_patch_stops(void);

// Save/reset/restore the loop-nesting depth, for forge_statement/lambda.c
// to wrap around a called body's compilation (same spirit as
// parser/H/declarations/vars.h's vars_save/vars_clear/vars_restore and each_loop.h's
// each_depth_save/reset/restore): a stop/next inside a function or
// lambda body must never reach past that body into an ENCLOSING loop it
// happens to be textually written inside.
void loop_depth_save(int *out);
void loop_depth_reset(void);
void loop_depth_restore(int saved);

#endif
