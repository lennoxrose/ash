#ifndef ASH_VM_HASHMAP_H
#define ASH_VM_HASHMAP_H
#include "value/H/value.h"

typedef struct {
    char *key;
    VMValue value;
    int used;
} VMMapEntry;

struct VMMap {
    VMMapEntry *entries;
    int count;
    int capacity;
};

VMMap *vm_map_new(void);
void vm_map_set(VMMap *m, char *key, VMValue value);
int vm_map_get(VMMap *m, const char *key, VMValue *out);
int vm_map_has(VMMap *m, const char *key);
void vm_map_delete(VMMap *m, const char *key);

#endif
