#ifndef ASH_VM_COMPILER_LOOP_STACK_H
#define ASH_VM_COMPILER_LOOP_STACK_H
#include "lexer/H/lexer.h"

// stop/next need to know which loop they belong to, and loops nest --
// they're pure "emit now, patch later" jump bookkeeping (same primitive
// OP_JUMP/OP_JUMP_IF_FALSE already use elsewhere in this compiler), just
// tracked per-nesting-level at compile time instead of resolved
// immediately. Mirrors kiln's own parser/C/statements/loop_stack.c.
//
// next can't simply backward-jump to a known offset the way a plain loop
// back-edge does: `next`'s target (the per-iteration advance step -- an
// each-loop's index increment, or straight back to the condition for a
// during loop) is only emitted AFTER the body compiles, i.e. after the
// next statement itself. So next jumps are forward-patched too, resolved
// by the loop's own codegen right after block() returns, before whatever
// per-iteration bookkeeping runs next.

// Pushes a fresh loop context (call right before compiling the body).
void loop_push(void);

// Called from stmt.c's stop_statement()/next_statement() -- records a
// forward jump's patch offset into the innermost loop's list. `keyword`
// anchors the "outside a loop" diagnostic at the stop/next token itself
// rather than wherever parsing happens to be next. Exits via
// diagnostics_report + exit(1) if called outside any loop.
void loop_record_stop(Token keyword, int patch_offset);
void loop_record_next(Token keyword, int patch_offset);

// Patches every recorded next jump to the CURRENT code position -- call
// right after block() returns, before emitting the per-iteration advance
// step.
void loop_patch_nexts(void);

// Patches every recorded stop jump to the CURRENT code position, then
// pops the loop context -- call once the loop is fully compiled.
void loop_pop_and_patch_stops(void);

// Save/reset/restore the loop-nesting depth, for compile_forge_decl and
// lambda_literal to wrap around a called body's compilation (same spirit
// as local_count's own save-recurse-restore): a function or lambda body
// gets its own Chunk at runtime, so a stop/next inside one must never
// reach past it into an ENCLOSING loop it happens to be textually written
// inside -- jumps can't cross Chunks anyway, so this is what turns that
// into a clean compile error instead of nonsense bytecode.
void loop_depth_save(int *out);
void loop_depth_reset(void);
void loop_depth_restore(int saved);

#endif
