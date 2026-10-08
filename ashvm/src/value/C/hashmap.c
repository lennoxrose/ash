#include <stdlib.h>
#include <string.h>
#include "value/H/hashmap.h"

#define INDEX_EMPTY (-1)
#define INDEX_DELETED (-2)

static unsigned long hash_string(const char *s) {
    unsigned long h = 5381;
    int c;
    while ((c = *s++)) h = ((h << 5) + h) + (unsigned long)c;
    return h;
}

static int *new_index(int capacity) {
    int *index = malloc(sizeof(int) * (size_t)capacity);
    for (int i = 0; i < capacity; i++) index[i] = INDEX_EMPTY;
    return index;
}

VMMap *vm_map_new(void) {
    VMMap *m = malloc(sizeof(VMMap));
    m->capacity = 8;
    m->size = 0;
    m->count = 0;
    m->entries = calloc((size_t)m->capacity, sizeof(VMMapEntry));
    m->index_capacity = 16;
    m->index = new_index(m->index_capacity);
    return m;
}

// Position in `entries` of `key`, or -1.
static int find_entry(VMMap *m, const char *key) {
    unsigned long h = hash_string(key) % (unsigned long)m->index_capacity;
    for (int probes = 0; probes < m->index_capacity; probes++) {
        int pos = m->index[h];
        if (pos == INDEX_EMPTY) return -1;
        if (pos >= 0 && strcmp(m->entries[pos].key, key) == 0) return pos;
        h = (h + 1) % (unsigned long)m->index_capacity;
    }
    return -1;
}

static void index_insert(VMMap *m, int pos) {
    unsigned long h = hash_string(m->entries[pos].key) % (unsigned long)m->index_capacity;
    while (m->index[h] >= 0) h = (h + 1) % (unsigned long)m->index_capacity;
    m->index[h] = pos;
}

// Called when `entries` is full: drop dead slots (keeping order), grow if the
// live entries still fill more than half, and rebuild the index.
static void compact_and_grow(VMMap *m) {
    int live = 0;
    for (int i = 0; i < m->size; i++) {
        if (m->entries[i].used) m->entries[live++] = m->entries[i];
    }
    m->size = live;
    if (live * 2 >= m->capacity) {
        m->capacity *= 2;
        m->entries = realloc(m->entries, sizeof(VMMapEntry) * (size_t)m->capacity);
    }
    for (int i = m->size; i < m->capacity; i++) m->entries[i].used = 0;
    free(m->index);
    m->index_capacity = m->capacity * 2;
    m->index = new_index(m->index_capacity);
    for (int i = 0; i < m->size; i++) index_insert(m, i);
}

void vm_map_set(VMMap *m, char *key, VMValue value) {
    int pos = find_entry(m, key);
    if (pos >= 0) {
        m->entries[pos].value = value;
        free(key);
        return;
    }
    if (m->size >= m->capacity) compact_and_grow(m);
    pos = m->size++;
    m->entries[pos].key = key;
    m->entries[pos].value = value;
    m->entries[pos].used = 1;
    m->count++;
    index_insert(m, pos);
}

int vm_map_get(VMMap *m, const char *key, VMValue *out) {
    int pos = find_entry(m, key);
    if (pos < 0) return 0;
    *out = m->entries[pos].value;
    return 1;
}

int vm_map_has(VMMap *m, const char *key) {
    return find_entry(m, key) >= 0;
}

void vm_map_delete(VMMap *m, const char *key) {
    unsigned long h = hash_string(key) % (unsigned long)m->index_capacity;
    for (int probes = 0; probes < m->index_capacity; probes++) {
        int pos = m->index[h];
        if (pos == INDEX_EMPTY) return;
        if (pos >= 0 && strcmp(m->entries[pos].key, key) == 0) {
            free(m->entries[pos].key);
            m->entries[pos].used = 0;
            m->index[h] = INDEX_DELETED;
            m->count--;
            return;
        }
        h = (h + 1) % (unsigned long)m->index_capacity;
    }
}
