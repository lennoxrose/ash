#include <stdlib.h>
#include <string.h>
#include "value/hashmap.h"

static unsigned long hash_string(const char *s) {
    unsigned long h = 5381;
    int c;
    while ((c = *s++)) h = ((h << 5) + h) + (unsigned long)c;
    return h;
}

VMMap *vm_map_new(void) {
    VMMap *m = malloc(sizeof(VMMap));
    m->capacity = 8;
    m->count = 0;
    m->entries = calloc(m->capacity, sizeof(VMMapEntry));
    return m;
}

static void vm_map_resize(VMMap *m) {
    int old_capacity = m->capacity;
    VMMapEntry *old_entries = m->entries;
    m->capacity *= 2;
    m->entries = calloc(m->capacity, sizeof(VMMapEntry));
    m->count = 0;
    for (int i = 0; i < old_capacity; i++) {
        if (old_entries[i].used) vm_map_set(m, old_entries[i].key, old_entries[i].value);
    }
    free(old_entries);
}

void vm_map_set(VMMap *m, char *key, VMValue value) {
    if ((double)(m->count + 1) / m->capacity > 0.7) vm_map_resize(m);
    unsigned long h = hash_string(key) % (unsigned long)m->capacity;
    while (m->entries[h].used) {
        if (strcmp(m->entries[h].key, key) == 0) {
            m->entries[h].value = value;
            free(key);
            return;
        }
        h = (h + 1) % (unsigned long)m->capacity;
    }
    m->entries[h].key = key;
    m->entries[h].value = value;
    m->entries[h].used = 1;
    m->count++;
}

int vm_map_get(VMMap *m, const char *key, VMValue *out) {
    unsigned long h = hash_string(key) % (unsigned long)m->capacity;
    unsigned long start = h;
    while (m->entries[h].used) {
        if (strcmp(m->entries[h].key, key) == 0) { *out = m->entries[h].value; return 1; }
        h = (h + 1) % (unsigned long)m->capacity;
        if (h == start) break;
    }
    return 0;
}

int vm_map_has(VMMap *m, const char *key) {
    VMValue tmp;
    return vm_map_get(m, key, &tmp);
}

void vm_map_delete(VMMap *m, const char *key) {
    unsigned long h = hash_string(key) % (unsigned long)m->capacity;
    unsigned long start = h;
    while (m->entries[h].used) {
        if (strcmp(m->entries[h].key, key) == 0) {
            free(m->entries[h].key);
            m->entries[h].used = 0;
            m->count--;
            return;
        }
        h = (h + 1) % (unsigned long)m->capacity;
        if (h == start) break;
    }
}
