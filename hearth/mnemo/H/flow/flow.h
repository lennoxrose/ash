#ifndef MNEMO_FLOW_H
#define MNEMO_FLOW_H

#include <stdint.h>

/* Mnemosyne move-flow engine (Rust, hearth/mnemo/Rust/src/flow.rs), shared by ashvm and kiln through
 * hearth/front's ownership pass.
 *
 * The caller walks a function body and reports events; the engine keeps the data-flow state: the set of
 * variable ids moved on the current path, whether the path is dead, and saved copies ("snapshots") used
 * to fork at a branch and join afterwards. Ids are opaque 64-bit values chosen by the caller (the
 * ownership pass uses the address of the variable).
 *
 * Ownership: mnemo_walk_new allocates a MnemoWalk that the caller frees exactly once with
 * mnemo_walk_free. Snapshot handles belong to their walk; 0 is never valid; release them with
 * mnemo_release (unreleased ones are reclaimed by mnemo_walk_free).
 * Errors: operations on a NULL walk or an unknown handle do nothing and return -1 (or 0 for queries);
 * the library never panics across this boundary.
 * Thread safety: a walk must not be used from two threads at once; separate walks are independent.
 * No pointer passed in is retained. */

typedef struct MnemoWalk MnemoWalk;

MnemoWalk *mnemo_walk_new(void);
void mnemo_walk_free(MnemoWalk *w);

void mnemo_move(MnemoWalk *w, uint64_t id);        /* id is moved on the current path (ignored if dead) */
void mnemo_reinit(MnemoWalk *w, uint64_t id);      /* id holds a new value again */
int mnemo_is_moved(MnemoWalk *w, uint64_t id);     /* 1 when id is moved on the current, live path */
void mnemo_kill(MnemoWalk *w);                     /* the current path ended */
int mnemo_is_dead(MnemoWalk *w);

uint32_t mnemo_save(MnemoWalk *w);                 /* snapshot of the current state */
uint32_t mnemo_save_dead(MnemoWalk *w);            /* snapshot of "no path reaches here" */
int mnemo_restore(MnemoWalk *w, uint32_t h);       /* current := snapshot */
int mnemo_join(MnemoWalk *w, uint32_t h);          /* current := current joined with snapshot */
int mnemo_join_into(MnemoWalk *w, uint32_t h);     /* snapshot := snapshot joined with current */
void mnemo_release(MnemoWalk *w, uint32_t h);

#endif
