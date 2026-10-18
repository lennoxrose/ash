#ifndef ASH_VM_HASHMAP_H
#define ASH_VM_HASHMAP_H
#include "value/H/value.h"

// Insertion-ordered map: `entries` is a dense list in the order keys were first
// set; `index` is an open-addressing table of positions into it, used only for
// lookup. Deleting leaves a dead slot (used == 0) so the order of the others
// is untouched; dead slots are squeezed out when the list fills up. Iterate
// `entries[0 .. size)` and skip entries with used == 0.
typedef struct {
    char *key;
    VMValue value;
    int used;
} VMMapEntry;

struct VMMap {
    VMMapEntry *entries;
    int size;       // slots filled in `entries`, live or dead
    int count;      // live entries
    int capacity;   // allocated slots in `entries`
    int *index;     // index_capacity slots: -1 empty, -2 deleted, else a position in entries
    int index_capacity;
};

VMMap *vm_map_new(void);
void vm_map_set(VMMap *m, char *key, VMValue value);
int vm_map_get(VMMap *m, const char *key, VMValue *out);
int vm_map_has(VMMap *m, const char *key);
void vm_map_delete(VMMap *m, const char *key);

#endif
