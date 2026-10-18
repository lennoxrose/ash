#ifndef KILN_MODULE_STATE_H
#define KILN_MODULE_STATE_H

// Module-level state: a top-level `local NAME = <expression>;` of the IR unit (see
// ir/H/ir_lex.h) whose value is not a plain number/string literal. The expression runs
// once, at program start before `start` is called, and the value lives in a numbered
// slot in the globals region instead of a frame, so every function of the unit can
// read it, and `NAME = ...;`, `NAME += ...;`, `NAME[i] = ...;` update it. Slots are
// named under the unit's namespace ("g.NAME"), like its functions and constants.

// Reserves a slot for NAME in the current import namespace; returns its index.
int module_state_declare(const char *name, int len);

// A bare NAME inside an imported file (resolved in that file's namespace), or
// a fully qualified "ns.NAME". Return the slot index or -1.
int module_state_resolve(const char *name, int len);
int module_state_resolve_qualified(const char *name, int len);

// Push the slot's (tag, payload) / pop a (tag, payload) into it.
void module_state_emit_load(int index);
void module_state_emit_store(int index);

#endif
