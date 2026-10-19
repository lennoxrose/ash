/* Value ranges through the public C ABI: set / get, join drops one-sided facts, widening, refine, iv_op. */
#include <stdint.h>
#include <stdio.h>
#include "mnemo/H/flow/flow.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

int main(void) {
    MnemoWalk *w = mnemo_walk_new();
    int64_t lo = 0, hi = 0;

    mnemo_rng_set(w, 1, 0, 0);
    mnemo_len_set(w, 2, 5, 5);
    CHECK(mnemo_rng_get(w, 1, &lo, &hi) == 1 && lo == 0 && hi == 0);
    CHECK(mnemo_len_get(w, 2, &lo, &hi) == 1 && lo == 5 && hi == 5);
    CHECK(mnemo_rng_get(w, 99, &lo, &hi) == 0);

    /* a fork: x = 0 on one side, 10 on the other; y only known on one side */
    uint32_t entry = mnemo_save(w);
    mnemo_rng_set(w, 3, 7, 7);
    mnemo_rng_set(w, 1, 10, 10);
    uint32_t other = mnemo_save(w);
    mnemo_restore(w, entry);
    mnemo_join(w, other);
    CHECK(mnemo_rng_get(w, 1, &lo, &hi) == 1 && lo == 0 && hi == 10);
    CHECK(mnemo_rng_get(w, 3, &lo, &hi) == 0);
    mnemo_release(w, entry);
    mnemo_release(w, other);

    /* loop head: i = 0, body makes it 1, widening sends the upper bound to infinity and then it is stable */
    mnemo_rng_set(w, 4, 0, 0);
    uint32_t head = mnemo_save(w);
    mnemo_rng_set(w, 4, 1, 1);
    uint32_t end = mnemo_save(w);
    mnemo_restore(w, head);
    mnemo_join_widen(w, end);
    CHECK(mnemo_rng_get(w, 4, &lo, &hi) == 1 && lo == 0 && hi == INT64_MAX);
    CHECK(mnemo_equal(w, head) == 0);
    mnemo_release(w, head);
    head = mnemo_save(w);
    CHECK(mnemo_equal(w, head) == 1);
    mnemo_release(w, head);
    mnemo_release(w, end);

    /* a condition i < 10 narrows; an impossible one ends the path */
    CHECK(mnemo_rng_refine(w, 4, INT64_MIN, 9) == 1);
    CHECK(mnemo_rng_get(w, 4, &lo, &hi) == 1 && lo == 0 && hi == 9);
    CHECK(mnemo_rng_refine(w, 4, 20, INT64_MAX) == 0);
    CHECK(mnemo_is_dead(w) == 1);
    CHECK(mnemo_rng_get(w, 4, &lo, &hi) == 0);

    /* unknown handles and NULL walks are harmless */
    CHECK(mnemo_join_widen(w, 12345) == -1);
    CHECK(mnemo_equal(NULL, 1) == 0);
    mnemo_rng_set(NULL, 1, 0, 0);
    CHECK(mnemo_rng_get(NULL, 1, &lo, &hi) == 0);
    mnemo_walk_free(w);

    /* interval arithmetic */
    int64_t a, b;
    CHECK(mnemo_iv_op('+', 1, 2, 10, 20, &a, &b) == 1 && a == 11 && b == 22);
    CHECK(mnemo_iv_op('*', -2, 3, 4, 5, &a, &b) == 1 && a == -10 && b == 15);
    CHECK(mnemo_iv_op('/', 10, 20, -1, 1, &a, &b) == 1 && a == -20 && b == 20);
    CHECK(mnemo_iv_op('/', 5, 5, 0, 0, &a, &b) == 3);
    CHECK(mnemo_iv_op('%', 0, 100, 1, 8, &a, &b) == 1 && a == 0 && b == 7);
    CHECK(mnemo_iv_op('+', INT64_MAX, INT64_MAX, 1, 1, &a, &b) == 2);
    CHECK(mnemo_iv_op('?', 0, 0, 0, 0, &a, &b) == 0);
    CHECK(mnemo_iv_op('+', 0, 0, 0, 0, NULL, &b) == 0);

    if (!failures) printf("range abi ok\n");
    return failures != 0;
}
