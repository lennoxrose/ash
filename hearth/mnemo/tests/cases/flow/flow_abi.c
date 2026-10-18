/* Calls the public C ABI of mnemo.a (H/flow/flow.h) the way front's ownership pass does. */
#include <stdio.h>
#include "mnemo/H/flow/flow.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

int main(void) {
    MnemoWalk *w = mnemo_walk_new();
    CHECK(w != NULL);

    /* move / reinit */
    mnemo_move(w, 1);
    CHECK(mnemo_is_moved(w, 1) == 1 && mnemo_is_moved(w, 2) == 0);
    mnemo_reinit(w, 1);
    CHECK(mnemo_is_moved(w, 1) == 0);

    /* branches: moved on one path stays moved after the join */
    uint32_t entry = mnemo_save(w);
    mnemo_move(w, 7);
    uint32_t then_s = mnemo_save(w);
    CHECK(mnemo_restore(w, entry) == 0 && mnemo_is_moved(w, 7) == 0);
    CHECK(mnemo_join(w, then_s) == 0 && mnemo_is_moved(w, 7) == 1);

    /* a dead path contributes nothing; a move on a dead path is ignored */
    uint32_t keep = mnemo_save(w);
    mnemo_restore(w, entry);
    mnemo_move(w, 9);
    mnemo_kill(w);
    CHECK(mnemo_is_dead(w) == 1 && mnemo_is_moved(w, 9) == 0);
    mnemo_move(w, 11);
    uint32_t dead_s = mnemo_save(w);
    mnemo_restore(w, keep);
    CHECK(mnemo_join(w, dead_s) == 0 && mnemo_is_moved(w, 7) == 1 && mnemo_is_moved(w, 9) == 0 && mnemo_is_moved(w, 11) == 0);

    /* accumulators: break states joined into a dead snapshot */
    uint32_t acc = mnemo_save_dead(w);
    CHECK(mnemo_join_into(w, acc) == 0);
    mnemo_restore(w, entry);
    mnemo_join(w, acc);
    CHECK(mnemo_is_moved(w, 7) == 1);

    /* handles: released and unknown ones are rejected without crashing */
    mnemo_release(w, acc);
    CHECK(mnemo_restore(w, acc) == -1 && mnemo_join(w, 0) == -1 && mnemo_join_into(w, 9999) == -1);
    mnemo_release(w, acc);
    mnemo_release(w, 9999);

    /* NULL walk is inert */
    mnemo_move(NULL, 1);
    CHECK(mnemo_is_moved(NULL, 1) == 0 && mnemo_save(NULL) == 0 && mnemo_restore(NULL, 1) == -1);
    mnemo_walk_free(NULL);

    mnemo_walk_free(w);
    if (failures) return 1;
    puts("flow ABI ok");
    return 0;
}
