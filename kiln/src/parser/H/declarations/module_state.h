#ifndef KILN_MODULE_STATE_H
#define KILN_MODULE_STATE_H

// Module-level state: a top-level `local NAME = <expression>;` in an imported
// file whose value is not a plain number/string literal. The expression runs
// once, where the `@import` appears (so before the importing file's own code),
// and the value lives in a numbered slot in the globals region instead of a
// frame, so every function of that file -- and `ns.NAME` from the importer --
// can read it, and `NAME = ...;`, `NAME += ...;`, `NAME[i] = ...;` update it.
// Slots are named under the file's namespace ("ns.NAME"), like imported
// functions and constants.

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
