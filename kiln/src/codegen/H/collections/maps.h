#ifndef KILN_MAPS_H
#define KILN_MAPS_H

// primary()'s '{' handling: parses `{"k0": v0, "k1": v1, ...}` (keys must
// be string literals, matching ashvm's own map-literal restriction),
// allocates, pushes (tag=TAG_MAP, payload=<map object address>).
void codegen_map_literal(void);

// Assumes key already pushed (top) and map already pushed (below it),
// both (tag, payload) pairs -- postfix()'s `m[k]` read. Dies with a
// clean error if the key isn't present (matching ashvm's "key not found").
void codegen_map_index_read(void);

// Assumes value pushed (top), key pushed (middle), map pushed (bottom)
// -- parser.c's `m[k] = v;` write. Updates in place if the key already
// exists, otherwise appends a new entry (growing if needed).
void codegen_map_index_store(void);

// Builtins, dispatched from codegen/C/builtins/builtins.c -- each assumes its
// arguments already pushed left to right, matching the normal
// call-argument convention. keys()/values() (each needs to build a new
// ARRAY sized only-known-at-runtime, real extra complexity for
// comparatively rare use) are deferred to milestone 10 alongside the
// rest of ashvm's remaining builtins -- not implemented here.
void codegen_builtin_has(void);    // has(map, key) -> NUMBER 1.0/0.0
void codegen_builtin_delete(void); // delete(map, key) -> the map (matches ashvm), no-op if key absent

#endif
