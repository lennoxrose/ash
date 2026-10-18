#ifdef ASH_GC
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "vm/H/gc.h"
#include "vm/H/state.h"
#include "value/H/hashmap.h"
#include "compiler/H/compiler.h"

/* Registration (gc.h) appends tagged pointers to gc_objs; the pointer -> index lookup table and the
 * mark bits exist only while a collection runs. */
uintptr_t *gc_objs;
size_t gc_n, gc_cap, gc_since, gc_trigger = 200000;  /* objects between collections */
int gc_pending, gc_inhibit;
static unsigned char *marks;       /* one byte per gc_objs entry, collection time only */
static size_t *table;              /* open addressing: index into gc_objs + 1, 0 = empty */
static size_t table_cap;
static int stress, stats;
static size_t n_collections, n_freed, n_registered;
static int initialised;

#define PTR(e) ((void *)((e) & ~(uintptr_t)15))
#define KIND(e) ((int)((e) & 15))

static size_t hash_ptr(void *p) {
    uintptr_t x = (uintptr_t)p;
    x ^= x >> 33; x *= 0xff51afd7ed558ccdULL; x ^= x >> 33;
    return (size_t)x;
}

static void gc_report(void) {
    fprintf(stderr, "gc: %zu collections, %zu objects registered, %zu freed, %zu live\n", n_collections, n_registered + gc_n, n_freed, gc_n);
}

static void init(void) {
    initialised = 1;
    const char *s = getenv("ASH_GC_STRESS");
    if (s && *s == '1') { stress = 1; gc_trigger = 1; }
    if (getenv("ASH_GC_STATS")) { stats = 1; atexit(gc_report); }
}

void gc_register_slow(void *p, int kind) {
    if (!p) return;
    if (!initialised) init();
    if (gc_n == gc_cap) {
        gc_cap = gc_cap ? gc_cap * 2 : 1 << 16;
        gc_objs = realloc(gc_objs, gc_cap * sizeof(uintptr_t));
    }
    gc_objs[gc_n++] = (uintptr_t)p | (uintptr_t)kind;
    if (++gc_since >= gc_trigger) gc_pending = 1;
}

/* collection-time lookup table over gc_objs; a buffer registered twice keeps only its first entry */
static void build_table(void) {
    table_cap = 16;
    while (table_cap < gc_n * 2) table_cap <<= 1;
    free(table);
    table = calloc(table_cap, sizeof(size_t));
    size_t kept = 0;
    for (size_t i = 0; i < gc_n; i++) {
        void *p = PTR(gc_objs[i]);
        size_t h = hash_ptr(p) & (table_cap - 1);
        int dup = 0;
        while (table[h]) {
            if (PTR(gc_objs[table[h] - 1]) == p) { dup = 1; break; }
            h = (h + 1) & (table_cap - 1);
        }
        if (dup) continue;
        gc_objs[kept] = gc_objs[i];
        table[h] = kept + 1;
        kept++;
    }
    n_freed += 0;
    gc_n = kept;
    /* entries moved while compacting duplicates: rebuild positions */
    memset(table, 0, table_cap * sizeof(size_t));
    for (size_t i = 0; i < gc_n; i++) {
        size_t h = hash_ptr(PTR(gc_objs[i])) & (table_cap - 1);
        while (table[h]) h = (h + 1) & (table_cap - 1);
        table[h] = i + 1;
    }
    free(marks);
    marks = calloc(gc_n ? gc_n : 1, 1);
}

static long find_index(void *p) {
    size_t h = hash_ptr(p) & (table_cap - 1);
    while (table[h]) {
        if (PTR(gc_objs[table[h] - 1]) == p) return (long)(table[h] - 1);
        h = (h + 1) & (table_cap - 1);
    }
    return -1;
}

/* ---- marking ---- */
static VMValue *work; static size_t work_n, work_cap;
static void push(VMValue v) {
    if (work_n == work_cap) { work_cap = work_cap ? work_cap * 2 : 1024; work = realloc(work, work_cap * sizeof(VMValue)); }
    work[work_n++] = v;
}

static int mark_ptr(void *p) {
    long i = find_index(p);
    if (i < 0 || marks[i]) return 0;
    marks[i] = 1;
    return 1;
}

static void visit(VMValue v) {
    switch (v.type) {
        case VM_STR: if (v.str) mark_ptr(v.str); break;
        case VM_ARRAY:
            if (v.array && mark_ptr(v.array)) for (int i = 0; i < v.array->count; i++) push(v.array->items[i]);
            break;
        case VM_MAP:
            if (v.map && mark_ptr(v.map)) {
                for (int i = 0; i < v.map->size; i++) if (v.map->entries[i].used) push(v.map->entries[i].value);
            }
            break;
        case VM_CLOSURE:
            if (v.closure && mark_ptr(v.closure)) for (int i = 0; i < v.closure->capture_count; i++) push(v.closure->captured[i]);
            break;
        default: break;
    }
}

static void drain(void) { while (work_n) visit(work[--work_n]); }
static void root(VMValue v) { visit(v); drain(); }

static void root_chunk(Chunk *c) {
    for (int i = 0; i < c->const_count; i++) root(c->constants[i]);
}

static void free_object(uintptr_t e) {
    void *p = PTR(e);
    switch (KIND(e)) {
        case GC_STR: free(p); break;
        case GC_ARRAY: { VMArray *a = p; free(a->items); free(a); break; }
        case GC_MAP: {
            VMMap *m = p;
            for (int i = 0; i < m->size; i++) if (m->entries[i].used) free(m->entries[i].key);
            free(m->entries); free(m->index); free(m);
            break;
        }
        case GC_CLOSURE: { VMClosureObj *c = p; free(c->captured); free(c); break; }
    }
}

void gc_collect(void) {
    gc_pending = 0;
    gc_since = 0;
    if (!initialised) init();
    n_collections++;
    build_table();
    /* roots: the whole value stack, module globals, chunk constants, the pending raised value */
    for (VMValue *v = stack; v < stack_top; v++) root(*v);
    for (int i = 0; i < MAX_VM_GLOBALS; i++) root(vm_globals[i]);
    if (has_raised_value) root(raised_value);
    for (int i = 0; i < frame_count; i++) root_chunk(frames[i].chunk);
    for (int i = 0; i < vm_function_count; i++) root_chunk(&vm_functions[i].chunk);
    /* sweep: free the unmarked, compact the survivors (mark cleared) */
    size_t live = 0;
    for (size_t i = 0; i < gc_n; i++) {
        if (marks[i]) gc_objs[live++] = gc_objs[i];
        else { free_object(gc_objs[i]); n_freed++; }
    }
    n_registered += gc_n - live + 0; /* freed ones stay counted in the totals */
    gc_n = live;
    free(table); table = NULL; table_cap = 0;
    free(marks); marks = NULL;
    if (!stress) gc_trigger = live * 2 > 200000 ? live * 2 : 200000;
}
#endif
