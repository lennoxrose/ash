#ifndef KILN_MAPS_INTERNAL_H
#define KILN_MAPS_INTERNAL_H

// Cross-file link between maps.c (literal, find_entry_index, read) and
// maps_mutate.c (store/has/delete -- everything that needs to find AND
// change an entry), not part of the public API (maps.h).

// INPUT: RSI=map object, RDI=target key payload -- both untouched by the
// loop until a match is found. On the "found" path (falls through),
// RBX=matching entry address. Returns the not-found jump's patch offset;
// the caller decides what "not found" means and must patch it.
int find_entry_index(void);

#endif
