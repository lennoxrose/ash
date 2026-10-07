#ifndef KILN_VARS_H
#define KILN_VARS_H
#include <stdint.h>

// Flat name -> stack-slot table for milestone 2's single-scope variable
// model (no nested block scoping yet -- see the plan for why that's
// deliberately deferred). Mirrors ashvm/src/compiler/state.c's
// resolve_local/declare_local, minus the per-function scope stack.
#define MAX_KILN_VARS 64

// Returns the slot index for `name`, or -1 if not yet declared.
int resolve_var(const char *name, int len);

// Declares a new variable and returns its slot index. Calls parse_error()
// (noreturn) if the fixed slot table is full.
int declare_var(const char *name, int len);

// Milestone 9: overwrites an already-declared slot's name in place (same
// slot index, new name) -- parser/C/statements/try_catch.c needs a catch variable's
// slot reserved BEFORE its real name (`catch (e)`) is parsed, since the
// slot's rbp-relative offset must be embedded into the handler struct at
// try-entry, ahead of the try block's body.
void rename_var(int slot, const char *name, int len);

// Milestone 5: each variable now holds a (tag, payload) pair (16 bytes,
// see codegen/H/emit/value.h), not a bare 8-byte double -- so each slot takes
// 16 bytes: tag at [rbp - 16*(i+1)], payload right after it at
// [rbp - 16*(i+1) + 8], below the frame pointer set up by parser.c's
// prologue (`mov rbp, rsp; sub rsp, MAX_KILN_VARS*16`). Shared here so
// expr.c and parser.c can't drift on the formula.
int32_t var_slot_tag_offset(int slot);
int32_t var_slot_payload_offset(int slot);

// Milestone 3: each function call gets its own fresh frame, so each
// function body needs its own fresh variable table too -- no visibility
// into the caller's (or top-level program's) variables yet (that needs
// closures, milestone 8). save/clear/restore lets parser.c's fn-statement
// codegen swap tables the same way ashvm's compile_fn_decl swaps
// local_names/local_count around compiling a function body.
typedef struct {
    char names[MAX_KILN_VARS][64];
    int count;
    int scope_start;
} VarScope;

void vars_save(VarScope *out);
void vars_clear(void);
void vars_restore(const VarScope *saved);

// Block scoping: `scope_start` marks the slot index where the innermost
// active block began. vars_scope_begin/end bracket a single block() call
// (or, for for/try statements whose own loop/catch variable is declared
// outside their nested block() call -- see for_loop.c/try_catch.c -- the
// whole statement). Same save-an-int-on-the-C-call-stack pattern as
// for_depth_save/restore and loop_depth_save/restore above: block() nests
// via ordinary recursive descent, so the C call stack gives correct
// nesting for free, no explicit stack structure needed.
typedef struct { int count_mark; int prev_scope_start; } VarBlockScope;

VarBlockScope vars_scope_begin(void);
void vars_scope_end(VarBlockScope saved);

// Like resolve_var, but only considers slots >= scope_start (the CURRENT
// block's own declarations). let_statement/for_statement use this for
// their "reuse this slot or declare fresh" decision: a name that only
// exists in an ENCLOSING scope must shadow (fresh slot), not mutate.
int resolve_var_in_current_scope(const char *name, int len);

// Milestone 8: one extra RBP-relative slot beyond the normal variable
// table, reserved for codegen/C/functions/closures.c's indirect-call codegen to stash
// a runtime-only capture_count across the `call` instruction (GP
// registers are all freely clobbered by the callee; RBP is the one thing
// provably restored to the caller's own value by the time any call
// returns, per the calling convention every milestone since M3 relies
// on).
#define KILN_CALL_SCRATCH_SIZE 16
int32_t call_scratch_offset(void);

// Milestone 10: a further 64-byte RBP-relative scratch region for
// map/filter/reduce's loop state (element pointer, count, running index,
// accumulator, output block address...) -- same "RBP survives a call,
// registers don't" reasoning as call_scratch_offset. Reused by whichever
// of the three is currently running with builtin-specific field meanings
// (see codegen/higher_order_*.c) since they're never concurrently active
// within the same frame -- source-level arguments are evaluated
// left-to-right, so a nested call like `map(map(a,f),g)` has the inner
// call's loop fully finish before the outer one's begins. A callback
// that itself calls another higher-order builtin is safe for a different
// reason: it runs in ITS OWN frame (its own rbp from its own call
// prologue), so its scratch use lands at a completely different address.
#define KILN_HIGHER_ORDER_SCRATCH_SIZE 128
int32_t higher_order_scratch_offset(void);

// Milestone 11: `for (x in array)` loop state (data_ptr, count, running
// index) also needs to survive across the loop body's own codegen -- but
// unlike try/catch's runtime recursion, for-loop NESTING is fully known
// at COMPILE TIME (nested `for` statements are literally nested calls in
// this single-pass compiler), so each nesting level just gets its own
// fixed offset rather than a runtime-tracked stack. Bounded the same way
// every other table here is; parser/C/statements/for_loop.c resets its nesting
// counter to 0 when entering a function or lambda body (mirroring
// vars_save/vars_clear/vars_restore), since that body gets its own RBP
// at runtime and can safely reuse the same offsets independently of how
// deep the enclosing code's own `for` nesting was.
#define MAX_KILN_FOR_DEPTH 8
#define KILN_FOR_LEVEL_SIZE 24
#define KILN_FOR_STACK_SIZE (MAX_KILN_FOR_DEPTH * KILN_FOR_LEVEL_SIZE)
int32_t for_level_offset(int depth);

// Plan A, phase A3: one 8-byte RBP-relative slot for codegen/C/platform/win_call.c to
// stash the original RSP across a Windows API call (dynamic 16-byte align
// + shadow space reservation clobbers RSP by an amount only known at
// runtime, so it can't be undone with a fixed `add rsp, N` -- see
// win_call.c). Same "RBP survives a call, registers don't, and each
// function frame gets its own copy" reasoning as every other scratch slot
// here; a single slot suffices since one win_call sequence always fully
// completes (there's no way for the compiled program to interleave two)
// before another can start.
#define KILN_WIN_CALL_SCRATCH_SIZE 8
int32_t win_call_scratch_offset(void);

// A second, separate slot: several Windows API calls (WriteFile,
// ReadFile) require a valid out-param pointer for a DWORD kiln has no use
// for (bytes actually written/read -- kiln already knows this from the
// length it requested, matching the "single read/write call assumed to
// fully succeed" simplification already documented in the Linux syscall
// call sites). Separate from win_call_scratch_offset because that slot is
// busy holding the saved RSP for the entire win_call_begin/end window;
// this one's only live transiently while such a call is being set up.
#define KILN_WIN_OUTPARAM_SCRATCH_SIZE 8
int32_t win_outparam_offset(void);

#define KILN_FRAME_RESERVE \
    ((MAX_KILN_VARS + 1) * 16 + KILN_HIGHER_ORDER_SCRATCH_SIZE + KILN_FOR_STACK_SIZE + \
     KILN_WIN_CALL_SCRATCH_SIZE + KILN_WIN_OUTPARAM_SCRATCH_SIZE)

#endif
