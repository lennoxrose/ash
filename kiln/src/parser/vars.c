#include <string.h>
#include "parser/vars.h"
#include "parser/parser.h"

static char var_names[MAX_KILN_VARS][64];
static int var_count = 0;
static int scope_start = 0;

// Scans newest-first (most-recently-declared wins), standard shadowing
// semantics -- required for try/catch: parser/try_catch.c reserves each
// catch variable's slot with a synthetic placeholder name and renames it
// to the real name only after the fact (the slot's rbp-offset has to be
// known before the try body compiles, before the real name is even
// parsed -- see try_catch.c's header comment), so two SEQUENTIAL (not
// nested) `catch (e)` blocks end up with two separate table entries both
// named "e". A first-match-wins scan would bind the second catch body's
// references to the FIRST catch's stale slot; newest-first fixes that
// while leaving `let`'s own reuse-if-already-declared behavior unchanged
// (there's normally only one live entry for a given `let` name anyway).
int resolve_var(const char *name, int len) {
    for (int i = var_count - 1; i >= 0; i--) {
        if ((int)strlen(var_names[i]) == len && strncmp(var_names[i], name, len) == 0) return i;
    }
    return -1;
}

int resolve_var_in_current_scope(const char *name, int len) {
    for (int i = var_count - 1; i >= scope_start; i--) {
        if ((int)strlen(var_names[i]) == len && strncmp(var_names[i], name, len) == 0) return i;
    }
    return -1;
}

int declare_var(const char *name, int len) {
    if (var_count >= MAX_KILN_VARS) parse_error("too many variables");
    memcpy(var_names[var_count], name, (size_t)len);
    var_names[var_count][len] = '\0';
    return var_count++;
}

int32_t var_slot_tag_offset(int slot) { return -16 * (slot + 1); }
int32_t var_slot_payload_offset(int slot) { return var_slot_tag_offset(slot) + 8; }

void vars_save(VarScope *out) {
    memcpy(out->names, var_names, sizeof(var_names));
    out->count = var_count;
    out->scope_start = scope_start;
}

void vars_clear(void) { var_count = 0; scope_start = 0; }

void vars_restore(const VarScope *saved) {
    memcpy(var_names, saved->names, sizeof(var_names));
    var_count = saved->count;
    scope_start = saved->scope_start;
}

VarBlockScope vars_scope_begin(void) {
    VarBlockScope saved = { .count_mark = var_count, .prev_scope_start = scope_start };
    scope_start = var_count;
    return saved;
}

void vars_scope_end(VarBlockScope saved) {
    var_count = saved.count_mark;
    scope_start = saved.prev_scope_start;
}

int32_t call_scratch_offset(void) { return var_slot_tag_offset(MAX_KILN_VARS); }

int32_t higher_order_scratch_offset(void) { return call_scratch_offset() - KILN_HIGHER_ORDER_SCRATCH_SIZE; }

int32_t for_level_offset(int depth) {
    return higher_order_scratch_offset() - KILN_FOR_STACK_SIZE + depth * KILN_FOR_LEVEL_SIZE;
}

int32_t win_call_scratch_offset(void) {
    return higher_order_scratch_offset() - KILN_FOR_STACK_SIZE - KILN_WIN_CALL_SCRATCH_SIZE;
}

int32_t win_outparam_offset(void) {
    return win_call_scratch_offset() - KILN_WIN_OUTPARAM_SCRATCH_SIZE;
}

void rename_var(int slot, const char *name, int len) {
    memcpy(var_names[slot], name, (size_t)len);
    var_names[slot][len] = '\0';
}
