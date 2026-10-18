#ifndef ASH_VM_GC_H
#define ASH_VM_GC_H

/* Optional mark-sweep garbage collector (EXPERIMENTAL, build with `make GC=1`).
 *
 * Without -DASH_GC every hook below is an empty macro, so the default build is byte-for-byte the
 * VM it always was (same bytecode dispatch, same performance). With it:
 *   - every heap object made by vm_str / vm_str_n / vm_array_new / vm_map_new / OP_MAKE_CLOSURE
 *     is registered in a pointer table (kind + mark bit);
 *   - the collector runs ONLY at safe points: the start of the opcodes that allocate (string concat,
 *     array/map literals, string indexing, closures, builtin calls). There every live value is on the
 *     VM stack, in a module global, in a chunk constant or in the pending raised value, so those are
 *     the complete root set; the call, loop and arithmetic paths have no check at all;
 *   - it never runs while a builtin has re-entered the VM (map/filter/reduce/sort callbacks keep
 *     C-local values), tracked by gc_inhibit;
 *   - pointers it did not allocate (static text, interior pointers) are ignored, never freed.
 * Ownership: strings are freed with free(), arrays/maps/closures with their own member frees.
 * ASH_GC_STRESS=1 collects at every safe point (finds missing roots); ASH_GC_STATS=1 prints totals
 * at exit. Not thread-safe (the VM is single-threaded). */

#ifdef ASH_GC
#include <stddef.h>
#include <stdint.h>
#include "value/H/value.h"

enum { GC_STR = 1, GC_ARRAY = 2, GC_MAP = 3, GC_CLOSURE = 4 };

extern int gc_pending;  /* set when enough was allocated since the last collection */
extern int gc_inhibit;  /* >0 while a builtin callback runs inside the VM */

/* The registry is a flat array of tagged pointers (malloc blocks are 16-byte aligned, so the low
 * bits carry the kind). The allocation path is inlined: one bounds check, one store, one counter. */
extern uintptr_t *gc_objs;
extern size_t gc_n, gc_cap, gc_since, gc_trigger;
void gc_register_slow(void *p, int kind);  /* first use, growth */
void gc_collect(void);

static inline void gc_register(void *p, int kind) {
    if (__builtin_expect(gc_n < gc_cap && p != NULL, 1)) {
        gc_objs[gc_n++] = (uintptr_t)p | (uintptr_t)kind;
        if (__builtin_expect(++gc_since >= gc_trigger, 0)) gc_pending = 1;
    } else {
        gc_register_slow(p, kind);
    }
}

#define GC_REGISTER(p, kind) gc_register((p), (kind))
/* Safe points sit at the START of opcodes that allocate (never on the call/loop/arithmetic paths, which
 * therefore carry no extra instruction). GC_SAFEPOINT_N(n) is for handlers that already popped n operands:
 * they are still intact above the stack top, so they are treated as live for the collection. */
#define GC_SAFEPOINT() do { if (__builtin_expect(gc_pending, 0) && !gc_inhibit) gc_collect(); } while (0)
#define GC_SAFEPOINT_N(n) do { if (__builtin_expect(gc_pending, 0) && !gc_inhibit) { stack_top += (n); gc_collect(); stack_top -= (n); } } while (0)
#define GC_INHIBIT_ENTER() (gc_inhibit++)
#define GC_INHIBIT_LEAVE() (gc_inhibit--)
#define GC_INHIBIT_SAVE(v) ((v) = gc_inhibit)
#define GC_INHIBIT_RESTORE(v) (gc_inhibit = (v))
#else
#define GC_REGISTER(p, kind) ((void)0)
#define GC_SAFEPOINT() ((void)0)
#define GC_SAFEPOINT_N(n) ((void)0)
#define GC_INHIBIT_ENTER() ((void)0)
#define GC_INHIBIT_LEAVE() ((void)0)
#define GC_INHIBIT_SAVE(v) ((void)0)
#define GC_INHIBIT_RESTORE(v) ((void)0)
#endif

#endif
