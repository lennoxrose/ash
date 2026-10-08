#ifndef ASH_VM_COMPILER_MODULE_STATE_H
#define ASH_VM_COMPILER_MODULE_STATE_H

// Module-level state: every top-level `local NAME = <expression>;` of an
// imported file. The expression runs once, where the `@import` appears (so
// before the importing file's own code), and the value lives in a numbered
// global slot instead of a frame, so every function of that file -- and
// `ns.NAME` from the importer -- can read it, and `NAME = ...;`, `NAME += ...;`,
// `NAME[i] = ...;` update it. Names are qualified with the file's namespace
// ("ns.NAME"), like imported functions.

// Reserves a slot for NAME in the current import namespace; returns its index.
int module_state_declare(const char *name, int len);
// A bare NAME inside an imported file (resolved in that file's namespace), or a
// fully qualified "ns.NAME". Return the slot index or -1.
int module_state_resolve(const char *name, int len);
int module_state_resolve_qualified(const char *name, int len);

#endif
