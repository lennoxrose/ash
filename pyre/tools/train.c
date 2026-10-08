/* `pyre train` / `pyre check`: teach the placement manager on THIS machine and grade it.
 *
 * Three ways to train, all grading themselves on sizes they did not train on:
 *   plain       a fixed grid of probes (--level/--step/--repeats), graded before and after;
 *   --auto      adaptive: grade, find where the manager's decisions are weakest, practise
 *               exactly there, grade again on FRESH held-out sizes, keep the round only if it
 *               did not make decisions worse, practise less as it converges, stop when it has;
 *   --forever   the same adaptive loop with no end: it keeps re-checking and re-practising
 *               (also catching drift: a GPU that got hotter, a driver update) until stopped.
 *
 * Grading never trains on its own data: each round measures a different set of held-out
 * sizes, every target really runs them, and both the previous and the candidate profile are
 * scored against the SAME measured times (so noise cannot make a round look better or worse
 * than it is).
 */
#define _POSIX_C_SOURCE 200809L
#include "train.h"
#include "json_out.h"
#include "H/runtime/manager.h"
#include <math.h>
#ifdef _OPENMP
#include <omp.h>
#endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    int step, repeats;
    double max_probe;     /* stop growing a target's size once one probe takes longer (s) */
    int max_exp;          /* largest size: 2^max_exp work units */
    double max_mem_mb;    /* skip probes that would move/hold more than this (0 = no cap) */
    double gpu_load;      /* 1..100: duty cycle on GPU targets */
    double cpu_load;      /* 1..100: duty cycle AND thread share on the CPU */
    double pause_ms;      /* fixed extra pause after every probe */
    int ops[PYRE_OP_COUNT], targets[PYRE_TARGET_COUNT];
    int reset, quiet, verify;
    const char *level;    /* preset name for the history log */
    int adaptive;         /* --auto */
    int forever;          /* --forever (implies adaptive) */
    int rounds;           /* --rounds N: adaptive rounds before it stops by itself */
    double round_pause_s; /* --round-pause S: idle between forever rounds */
} TrainOpts;

static volatile sig_atomic_t g_stop;
static void on_stop(int sig) { (void)sig; g_stop = 1; }

/* ---------- problem shapes and one timed run ---------- */

typedef struct { int a, b; PyreWorkload w; char text[48]; } Shape;

/* The concrete problem size for ~2^exp work units of this kernel. */
static Shape shape_of(PyreOp op, int exp) {
    Shape s = {0};
    double work = ldexp(1.0, exp);
    if (op == PYRE_OP_MATMUL) {
        s.a = (int)llround(cbrt(work));
        if (s.a < 4) s.a = 4;
        s.w = pyre_workload_matmul(s.a);
        snprintf(s.text, sizeof(s.text), "n=%d", s.a);
    } else if (op == PYRE_OP_CHAOS) {
        double nd = work / 200.0;
        s.a = nd < 1000 ? 1000 : nd > 2000000 ? 2000000 : (int)nd;
        s.b = (int)fmax(1.0, work / s.a);
        s.w = pyre_workload_chaos(s.a, s.b);
        snprintf(s.text, sizeof(s.text), "n=%d it=%d", s.a, s.b);
    } else {
        s.w = pyre_workload_monte_carlo((long long)work);
        snprintf(s.text, sizeof(s.text), "iters=%lld", (long long)work);
    }
    return s;
}

/* Runs the shape on a target; seconds, or < 0 if it could not run. */
static double run_shape(PyreOp op, PyreTarget t, const Shape *s) {
    double sec = -1.0;
    if (op == PYRE_OP_MATMUL) {
        size_t cnt = (size_t)s->a * s->a;
        double *a = malloc(cnt * 8), *b = malloc(cnt * 8), *c = malloc(cnt * 8);
        if (a && b && c) {
            for (size_t i = 0; i < cnt; i++) { a[i] = (double)(i % 13) * 0.5; b[i] = (double)(i % 7) * 1.5; }
            double t0 = pyre_now_seconds();
            if (pyre_run_matmul(t, a, b, c, s->a)) sec = pyre_now_seconds() - t0;
        }
        free(a); free(b); free(c);
    } else if (op == PYRE_OP_CHAOS) {
        double *d = malloc((size_t)s->a * 8);
        if (d) {
            for (int i = 0; i < s->a; i++) d[i] = (double)(i % 1000) * 0.001;
            double t0 = pyre_now_seconds();
            if (pyre_run_chaos(t, d, s->a, s->b)) sec = pyre_now_seconds() - t0;
        }
        free(d);
    } else {
        long long risky;
        double p;
        double t0 = pyre_now_seconds();
        if (pyre_run_monte_carlo(t, (long long)s->w.work, 0.75, 1u, &risky, &p)) sec = pyre_now_seconds() - t0;
    }
    return sec;
}

/* Sleeps in short slices so a stop request is honoured within a fraction of a second. */
static void sleep_interruptible(double seconds) {
    while (seconds > 0 && !g_stop) {
        double slice = seconds < 0.1 ? seconds : 0.1;
        struct timespec ts = {(time_t)slice, (long)((slice - (double)(time_t)slice) * 1e9)};
        nanosleep(&ts, NULL);
        seconds -= slice;
    }
}

/* Duty-cycle throttle: after a probe that kept the device busy for `busy` seconds, idle long
 * enough that busy / (busy + idle) == load%. Caps the AVERAGE share of the device training
 * occupies; it cannot shrink a single probe (use --max-probe / --max-work / --max-mem). */
static void throttle(double busy, double load, double pause_ms) {
    double idle = load < 100.0 ? busy * (100.0 / load - 1.0) : 0.0;
    idle += pause_ms / 1e3;
    sleep_interruptible(idle);
}

/* ---------- grading: measure real times once, score any profile against them ---------- */

typedef struct {
    int shapes, best_hits;
    double lost_sum;     /* seconds lost to picking a slower target than the best */
    double best_sum;     /* seconds the best targets would have taken */
    double err_sum;      /* sum of |predicted - actual| / actual for the pick */
    int err_n;
} Score;

static double mean_regret(const Score *s) { return s->best_sum > 0 ? s->lost_sum / s->best_sum : 0.0; }
static double mean_error(const Score *s) { return s->err_n ? s->err_sum / s->err_n : 0.0; }
/* One number for "how good are the decisions": time lost, plus a light penalty for wrong predictions. */
static double cost(const Score *s) { return mean_regret(s) + 0.1 * mean_error(s); }

#define MAX_ROWS 48
typedef struct {
    int op, exp;
    Shape sh;
    double actual[PYRE_TARGET_COUNT];
    int assumed[PYRE_TARGET_COUNT];  /* not run: already past --max-probe, charged at twice that */
} MRow;
typedef struct { int n; MRow row[MAX_ROWS]; } Measured;

typedef struct { int op, exp; double priority; } Weak;  /* where the manager was wrong, worst first */

static void print_score(const char *label, const Score *s);

/* Really runs every chosen target on held-out sizes (between the sizes the grid trains).
 * `offset` shifts which sizes, so successive rounds never grade on the same ones. */
static void measure(const TrainOpts *o, double offset, Measured *m) {
    m->n = 0;
    for (int op = 0; op < PYRE_OP_COUNT; op++) {
        if (!o->ops[op]) continue;
        int slow[PYRE_TARGET_COUNT] = {0};
        for (double e = 13.5 + offset; e <= o->max_exp; e += 4.0) {
            if (g_stop || m->n >= MAX_ROWS) return;
            MRow *r = &m->row[m->n];
            memset(r, 0, sizeof(*r));
            r->op = op;
            r->exp = (int)e;
            r->sh = shape_of(op, r->exp);
            int any = 0;
            for (int t = 0; t < PYRE_TARGET_COUNT; t++) {
                r->actual[t] = -1.0;
                if (!o->targets[t] || !pyre_target_is_up(t) || !pyre_target_supports(t, op)) continue;
                if (slow[t]) { r->actual[t] = o->max_probe * 2.0; r->assumed[t] = 1; continue; }
                /* Twice, keep the faster: the first run pays cold-start costs. */
                r->actual[t] = run_shape(op, t, &r->sh);
                if (r->actual[t] > 0 && r->actual[t] <= o->max_probe) {
                    double again = run_shape(op, t, &r->sh);
                    if (again > 0 && again < r->actual[t]) r->actual[t] = again;
                }
                if (r->actual[t] > o->max_probe) slow[t] = 1;
                throttle(r->actual[t] > 0 ? r->actual[t] : 0, t == PYRE_CPU ? o->cpu_load : o->gpu_load, o->pause_ms);
                any = 1;
            }
            if (any) m->n++;
        }
    }
}

/* Scores whatever profile the manager currently holds against measured times. */
static Score score_measured(const Measured *m, const char *phase, int verbose, Weak *weak, int *nweak) {
    Score sc = {0};
    if (nweak) *nweak = 0;
    for (int i = 0; i < m->n; i++) {
        const MRow *r = &m->row[i];
        double best = 1e30;
        for (int t = 0; t < PYRE_TARGET_COUNT; t++) if (r->actual[t] > 0 && r->actual[t] < best) best = r->actual[t];
        double est[PYRE_TARGET_COUNT];
        PyreTarget pick = pyre_manager_estimate(r->op, r->sh.w, est);
        if (r->actual[pick] <= 0) continue;  /* picked something this run did not measure */
        /* Right = within 5% of the best, or within 0.2 ms of it (below that, timing noise
         * outweighs any difference worth caring about). */
        int right = r->actual[pick] <= best * 1.05 || r->actual[pick] - best < 2e-4;
        double lost = r->actual[pick] - best;
        sc.shapes++;
        sc.lost_sum += lost;
        sc.best_sum += best;
        if (right) sc.best_hits++;
        double err = 0.5;  /* never measured at this size: treat as a poor prediction */
        if (est[pick] >= 0 && !r->assumed[pick]) {
            err = fabs(est[pick] - r->actual[pick]) / r->actual[pick];
            sc.err_sum += err;
            sc.err_n++;
        }
        if (weak && nweak && *nweak < MAX_ROWS) weak[(*nweak)++] = (Weak){r->op, r->exp, lost / best + 0.5 * err + (right ? 0.0 : 0.5)};

        if (json_mode) {
            jbegin("eval_row");
            jkey("phase"); jstr(phase);
            jkey("op"); jstr(pyre_op_name(r->op));
            jkey("shape"); jstr(r->sh.text);
            jkey("times_ms"); printf("{");
            int first = 1;
            for (int t = 0; t < PYRE_TARGET_COUNT; t++) {
                if (r->actual[t] > 0 && !r->assumed[t]) { printf("%s\"%s\":%.6g", first ? "" : ",", pyre_target_name(t), r->actual[t] * 1e3); first = 0; }
            }
            printf("}");
            jkey("picked"); jstr(pyre_target_name(pick));
            if (est[pick] >= 0) { jkey("predicted_ms"); printf("%.6g", est[pick] * 1e3); }
            jkey("right"); printf("%s", right ? "true" : "false");
            jend();
        } else if (verbose) {
            printf("  %-17s %-22s", pyre_op_name(r->op), r->sh.text);
            for (int t = 0; t < PYRE_TARGET_COUNT; t++) if (r->actual[t] > 0 && !r->assumed[t]) printf("  %s %.4gms", pyre_target_name(t), r->actual[t] * 1e3);
            printf("  | picked %s", pyre_target_name(pick));
            if (est[pick] >= 0) printf(" (predicted %.4gms)", est[pick] * 1e3);
            printf("%s\n", right ? "" : "   <-- WRONG");
        }
    }
    (void)phase;
    return sc;
}

static void print_score(const char *label, const Score *s) {
    if (json_mode) {
        jbegin("score");
        jkey("phase"); jstr(label);
        jkey("shapes"); printf("%d", s->shapes);
        jkey("hits"); printf("%d", s->best_hits);
        jkey("lost_pct"); printf("%.4f", mean_regret(s) * 100.0);
        jkey("error_pct"); if (s->err_n) printf("%.4f", mean_error(s) * 100.0); else printf("null");
        jend();
        return;
    }
    if (!s->shapes) { printf("  %-7s no held-out sizes could be judged\n", label); return; }
    printf("  %-7s time lost to wrong placement: %5.1f%%   picked the best: %d/%d", label, mean_regret(s) * 100.0, s->best_hits, s->shapes);
    if (s->err_n) printf("   prediction error: %.0f%%", mean_error(s) * 100.0);
    else printf("   prediction error: n/a (nothing measured yet)");
    printf("\n");
}

/* ---------- history: one JSON line per finished train/check, next to the profile ---------- */

static void append_history(const char *kind, const TrainOpts *o, const Score *before, const Score *after,
                           const char *verdict, int probes, double seconds) {
    const char *prof = pyre_manager_profile_path();
    if (!*prof) return;
    char path[1100];
    snprintf(path, sizeof(path), "%s", prof);
    char *slash = strrchr(path, '/');
    snprintf(slash ? slash + 1 : path, sizeof(path) - (slash ? (size_t)(slash + 1 - path) : 0), "history.jsonl");
    FILE *f = fopen(path, "a");
    if (!f) return;
    const Score *sc[2] = {before, after};
    const char *names[2] = {"before", "after"};
    fprintf(f, "{\"time\":%lld,\"kind\":\"%s\",\"level\":\"%s\",\"verdict\":\"%s\",\"probes\":%d,\"seconds\":%.2f,"
               "\"gpu_load\":%g,\"cpu_load\":%g,\"step\":%d,\"repeats\":%d",
            (long long)time(NULL), kind, o->level, verdict, probes, seconds, o->gpu_load, o->cpu_load, o->step, o->repeats);
    for (int i = 0; i < 2; i++) {
        if (!sc[i] || !sc[i]->shapes) continue;
        fprintf(f, ",\"%s\":{\"shapes\":%d,\"hits\":%d,\"lost_pct\":%.4f,\"error_pct\":", names[i], sc[i]->shapes, sc[i]->best_hits, mean_regret(sc[i]) * 100.0);
        if (sc[i]->err_n) fprintf(f, "%.4f}", mean_error(sc[i]) * 100.0); else fprintf(f, "null}");
    }
    fprintf(f, "}\n");
    fclose(f);
}

/* ---------- practising: timed probes that update the profile ---------- */

/* `repeats` real runs at ~2^exp units of `op` on target `t`, folded into the profile.
 * Returns the slowest run in seconds, -1 if it could not run, -2 if the size is over --max-mem. */
static double probe_exp(const TrainOpts *o, PyreProfile *p, int op, int t, int exp, int repeats, int *probes) {
    Shape sh = shape_of(op, exp);
    if (o->max_mem_mb > 0 && sh.w.bytes / 1048576.0 > o->max_mem_mb) return -2.0;
    double load = t == PYRE_CPU ? o->cpu_load : o->gpu_load;
    double slowest = 0.0;
    for (int rep = 0; rep < repeats && !g_stop; rep++) {
        double sec = run_shape(op, t, &sh);
        if (sec < 0) return -1.0;
        pyre_rates_observe(&p->rates[op][t], sh.w.work, sec);
        (*probes)++;
        if (sec > slowest) slowest = sec;
        if (json_mode) {
            jbegin("probe");
            jkey("op"); jstr(pyre_op_name(op));
            jkey("target"); jstr(pyre_target_name(t));
            jkey("exp"); printf("%d", exp);
            jkey("shape"); jstr(sh.text);
            jkey("ms"); printf("%.6g", sec * 1e3);
            jend();
        } else if (!o->quiet) {
            printf("  %-17s %-7s 2^%-2d %-22s %10.4f ms\n", pyre_op_name(op), pyre_target_name(t), exp, sh.text, sec * 1e3);
        }
        throttle(sec, load, o->pause_ms);
    }
    return slowest;
}

static void record_target(PyreProfile *p, int t) {
    if (t == PYRE_CPU) return;
    p->present[t] = 1;
    snprintf(p->device[t], sizeof(p->device[t]), "%s", pyre_target_device(t));
    p->init_seconds[t] = pyre_target_init_seconds(t);
}

/* The plain grid: every chosen kernel on every chosen target at every `step`-th size. */
static void train_grid(const TrainOpts *o, PyreProfile *p, int *probes) {
    for (int t = 0; t < PYRE_TARGET_COUNT && !g_stop; t++) {
        if (!o->targets[t]) continue;
        if (!pyre_target_init(t)) {
            if (json_mode) { jbegin("skipped"); jkey("target"); jstr(pyre_target_name(t)); jend(); }
            else printf("%s: not available, skipped\n", pyre_target_name(t));
            continue;
        }
        record_target(p, t);
        for (int op = 0; op < PYRE_OP_COUNT && !g_stop; op++) {
            if (!o->ops[op] || !pyre_target_supports(t, op)) continue;
            Shape warm = shape_of(op, 12);
            run_shape(op, t, &warm);  /* warm-up, discarded */
            for (int exp = 12; exp <= o->max_exp && !g_stop; exp += o->step) {
                double slowest = probe_exp(o, p, op, t, exp, o->repeats, probes);
                if (slowest < 0 || slowest > o->max_probe) break;  /* bigger only gets slower: enough for this device */
            }
        }
    }
}

/* Practise on every usable target at the sizes around `center` (the weak spot's neighbourhood). */
static void practise(const TrainOpts *o, PyreProfile *p, int op, int center, int repeats, int *probes) {
    for (int t = 0; t < PYRE_TARGET_COUNT && !g_stop; t++) {
        if (!o->targets[t] || !pyre_target_is_up(t) || !pyre_target_supports(t, op)) continue;
        for (int exp = center - 1; exp <= center + 1 && !g_stop; exp++) {
            if (exp < 12 || exp > o->max_exp) continue;
            double slowest = probe_exp(o, p, op, t, exp, repeats, probes);
            if (slowest < 0 || slowest > o->max_probe) break;
        }
    }
}

/* ---------- the adaptive learner (--auto / --forever) ---------- */

static int has_learned(const TrainOpts *o, const PyreProfile *p) {
    for (int op = 0; op < PYRE_OP_COUNT; op++) {
        if (!o->ops[op]) continue;
        for (int t = 0; t < PYRE_TARGET_COUNT; t++) {
            if (!o->targets[t]) continue;
            for (int b = 0; b < PYRE_BUCKETS; b++) if (p->rates[op][t].seen[b]) return 1;
        }
    }
    return 0;
}

static int by_priority(const void *a, const void *b) {
    double x = ((const Weak *)a)->priority, y = ((const Weak *)b)->priority;
    return x < y ? 1 : x > y ? -1 : 0;
}

static void emit_round(int round, const Score *s, int accepted, int probes, const Weak *focus, int nfocus, double cost_before, double cost_after) {
    if (!json_mode) {
        printf("round %d: time lost %.1f%%, right %d/%d, error %.0f%% -> %s (%d probes)\n", round, mean_regret(s) * 100.0,
               s->best_hits, s->shapes, mean_error(s) * 100.0, round == 0 ? "baseline" : accepted ? "kept" : "reverted", probes);
        return;
    }
    jbegin("round");
    jkey("round"); printf("%d", round);
    jkey("lost_pct"); printf("%.4f", mean_regret(s) * 100.0);
    jkey("error_pct"); if (s->err_n) printf("%.4f", mean_error(s) * 100.0); else printf("null");
    jkey("hits"); printf("%d", s->best_hits);
    jkey("shapes"); printf("%d", s->shapes);
    jkey("accepted"); printf("%s", accepted ? "true" : "false");
    jkey("probes"); printf("%d", probes);
    jkey("cost_before"); printf("%.5f", cost_before);
    jkey("cost_after"); printf("%.5f", cost_after);
    jkey("focus"); printf("[");
    for (int i = 0; i < nfocus; i++) {
        printf("%s", i ? "," : "");
        char buf[64];
        snprintf(buf, sizeof(buf), "%s 2^%d", pyre_op_name(focus[i].op), focus[i].exp);
        jstr(buf);
    }
    printf("]");
    jend();
}

/* Grade, practise where it is weakest, grade again on fresh sizes, keep it only if no worse.
 * Returns the exit code. */
static int run_adaptive(TrainOpts *o, PyreProfile *p) {
    double started = pyre_now_seconds();
    int probes = 0;
    for (int t = 1; t < PYRE_TARGET_COUNT; t++) if (o->targets[t]) pyre_target_init(t);
    if (json_mode) { jbegin("mode"); jkey("mode"); jstr(o->forever ? "forever" : "auto"); jend(); }

    if (!has_learned(o, p)) {  /* nothing to be weak at yet: lay down a grid first */
        if (json_mode) { jbegin("phase"); jkey("name"); jstr("seeding"); jend(); }
        else printf("nothing learned yet: seeding with a grid first\n");
        train_grid(o, p, &probes);
    }

    PyreProfile accepted = *p;
    Score first = {0}, last = {0};
    int converged = 0, round = 0, kept = 0;

    for (round = 0; !g_stop; round++) {
        if (!o->forever && round > o->rounds) break;
        if (json_mode) { jbegin("phase"); jkey("name"); jstr(round == 0 ? "check-before" : "practising"); jend(); }

        /* Fresh held-out sizes every round, measured once; the accepted profile is the baseline. */
        Measured m;
        measure(o, (double)(round % 4), &m);
        if (g_stop) break;
        Weak weak[MAX_ROWS];
        int nweak = 0;
        Score before = score_measured(&m, round == 0 ? "before" : "round-before", 0, weak, &nweak);
        if (round == 0) {
            first = before;
            last = before;
            print_score("before", &before);
            emit_round(0, &before, 1, probes, NULL, 0, cost(&before), cost(&before));
            continue;
        }

        qsort(weak, (size_t)nweak, sizeof(Weak), by_priority);
        Weak focus[3];
        int nfocus = 0;
        for (int i = 0; i < nweak && nfocus < 3; i++) {
            int dup = 0;
            for (int j = 0; j < nfocus; j++) if (focus[j].op == weak[i].op && focus[j].exp == weak[i].exp) dup = 1;
            if (!dup) focus[nfocus++] = weak[i];
        }
        /* Practice shrinks as it converges (like a decaying learning rate), but never to nothing. */
        int floor_reps = o->forever ? 2 : 1;
        int reps = (int)ceil(o->repeats * pow(0.75, round - 1));
        if (reps < floor_reps) reps = floor_reps;
        int round_probes = 0;
        for (int i = 0; i < nfocus && !g_stop; i++) practise(o, p, focus[i].op, focus[i].exp, reps, &round_probes);
        probes += round_probes;
        if (g_stop) break;

        Score after = score_measured(&m, "round-after", 0, NULL, NULL);
        double cb = cost(&before), ca = cost(&after);
        int accept = ca <= cb + 0.002;
        if (accept) { accepted = *p; kept++; last = after; }
        else { *p = accepted; last = before; }
        emit_round(round, accept ? &after : &before, accept, round_probes, focus, nfocus, cb, ca);

        if (accept && !o->forever) {
            /* Converged: two rounds in a row that improve by under 0.3 points. */
            converged = (cb - ca < 0.003) ? converged + 1 : 0;
            if (converged >= 2) { if (json_mode) { jbegin("converged"); jkey("round"); printf("%d", round); jend(); } else printf("converged\n"); break; }
        }
        if (o->forever) {
            if (accept) pyre_manager_save();  /* the dashboard sees learning as it happens */
            if (json_mode) { jbegin("idle"); jkey("seconds"); printf("%.0f", o->round_pause_s); jend(); }
            sleep_interruptible(o->round_pause_s);
        }
    }

    *p = accepted;  /* a stop mid-round must not leave half-practised data */
    double seconds = pyre_now_seconds() - started;
    const char *verdict = cost(&last) < cost(&first) - 0.02 ? "better" : "same";
    if (!first.shapes || !last.shapes) verdict = "unknown";
    int saved = pyre_manager_save();
    if (json_mode) {
        jbegin("verdict"); jkey("verdict"); jstr(verdict); jkey("discarded"); printf("false"); jend();
        jbegin("done"); jkey("probes"); printf("%d", probes); jkey("seconds"); printf("%.2f", seconds); jkey("saved"); printf("%s", saved ? "true" : "false"); jend();
    } else {
        printf("done after %d rounds (%d kept), %d probes in %.1fs: %s\n", round, kept, probes, seconds, verdict);
        print_score("before", &first);
        print_score("after", &last);
    }
    o->level = o->forever ? "forever" : "auto";
    append_history("train", o, &first, &last, verdict, probes, seconds);
    return 0;
}

/* ---------- commands ---------- */

static int parse_list(const char *csv, const char *const names[], int count, int out[]) {
    memset(out, 0, sizeof(int) * count);
    char buf[128];
    snprintf(buf, sizeof(buf), "%s", csv);
    for (char *tok = strtok(buf, ","); tok; tok = strtok(NULL, ",")) {
        int found = 0;
        for (int i = 0; i < count; i++) if (!strcmp(tok, names[i])) { out[i] = 1; found = 1; }
        if (!found) { fprintf(stderr, "pyre: unknown name '%s' (try `pyre help train`)\n", tok); return 0; }
    }
    return 1;
}

static int bad_flag(const char *flag, const char *why) {
    fprintf(stderr, "pyre: %s %s (try `pyre help train`)\n", flag, why);
    return 2;
}

int cmd_train(int argc, char **argv) {
    static const char *const op_names[PYRE_OP_COUNT] = {"matmul", "chaos", "mc"};
    static const char *const target_names[PYRE_TARGET_COUNT] = {"cpu", "cuda", "vulkan"};
    TrainOpts o = {.step = 2, .repeats = 2, .max_probe = 1.0, .max_exp = 34, .gpu_load = 100, .cpu_load = 100, .verify = 1,
                   .level = "normal", .rounds = 6, .round_pause_s = 20};
    for (int i = 0; i < PYRE_OP_COUNT; i++) o.ops[i] = 1;
    for (int i = 0; i < PYRE_TARGET_COUNT; i++) o.targets[i] = 1;
    int probe_set = 0;

    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        const char *v = i + 1 < argc ? argv[i + 1] : NULL;
        int flag_only = !strcmp(a, "--reset") || !strcmp(a, "--quiet") || !strcmp(a, "--no-verify") || !strcmp(a, "--auto") || !strcmp(a, "--forever");
        if (!flag_only && !v) return bad_flag(a, "needs a value");
        if (!strcmp(a, "--level")) {
            if (!strcmp(v, "quick")) { o.level = "quick"; o.step = 3; o.repeats = 1; if (!probe_set) o.max_probe = 0.25; }
            else if (!strcmp(v, "normal")) { o.level = "normal"; o.step = 2; o.repeats = 2; if (!probe_set) o.max_probe = 1.0; }
            else if (!strcmp(v, "deep")) { o.level = "deep"; o.step = 1; o.repeats = 3; if (!probe_set) o.max_probe = 3.0; }
            else return bad_flag(a, "is quick|normal|deep");
        } else if (!strcmp(a, "--ops")) { if (!parse_list(v, op_names, PYRE_OP_COUNT, o.ops)) return 2; }
        else if (!strcmp(a, "--targets")) { if (!parse_list(v, target_names, PYRE_TARGET_COUNT, o.targets)) return 2; }
        else if (!strcmp(a, "--gpu-load")) { o.gpu_load = atof(v); if (o.gpu_load < 1 || o.gpu_load > 100) return bad_flag(a, "is 1..100"); }
        else if (!strcmp(a, "--cpu-load")) { o.cpu_load = atof(v); if (o.cpu_load < 1 || o.cpu_load > 100) return bad_flag(a, "is 1..100"); }
        else if (!strcmp(a, "--max-probe")) { o.max_probe = atof(v); probe_set = 1; if (o.max_probe <= 0) return bad_flag(a, "must be > 0"); }
        else if (!strcmp(a, "--max-work")) { o.max_exp = atoi(v); if (o.max_exp < 12 || o.max_exp > 40) return bad_flag(a, "is 12..40"); }
        else if (!strcmp(a, "--max-mem")) { o.max_mem_mb = atof(v); if (o.max_mem_mb < 0) return bad_flag(a, "must be >= 0"); }
        else if (!strcmp(a, "--repeats")) { o.repeats = atoi(v); if (o.repeats < 1 || o.repeats > 50) return bad_flag(a, "is 1..50"); }
        else if (!strcmp(a, "--step")) { o.step = atoi(v); if (o.step < 1 || o.step > 12) return bad_flag(a, "is 1..12"); }
        else if (!strcmp(a, "--pause")) { o.pause_ms = atof(v); if (o.pause_ms < 0) return bad_flag(a, "must be >= 0"); }
        else if (!strcmp(a, "--rounds")) { o.rounds = atoi(v); if (o.rounds < 1 || o.rounds > 1000) return bad_flag(a, "is 1..1000"); }
        else if (!strcmp(a, "--round-pause")) { o.round_pause_s = atof(v); if (o.round_pause_s < 0 || o.round_pause_s > 86400) return bad_flag(a, "is 0..86400 seconds"); }
        else if (!strcmp(a, "--reset")) { o.reset = 1; continue; }
        else if (!strcmp(a, "--quiet")) { o.quiet = 1; continue; }
        else if (!strcmp(a, "--no-verify")) { o.verify = 0; continue; }
        else if (!strcmp(a, "--auto")) { o.adaptive = 1; continue; }
        else if (!strcmp(a, "--forever")) { o.adaptive = o.forever = 1; continue; }
        else return bad_flag(a, "is not a train flag");
        i++;
    }
    if (o.reset) pyre_manager_forget();

#ifdef _OPENMP
    if (o.cpu_load < 100) {
        int n = (int)(omp_get_max_threads() * o.cpu_load / 100.0);
        omp_set_num_threads(n < 1 ? 1 : n);
    }
#endif
    struct sigaction sa = {.sa_handler = on_stop};  /* Stop in the dashboard sends SIGTERM: finish tidily */
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    PyreProfile *p = pyre_manager_profile_edit();
    if (o.adaptive) return run_adaptive(&o, p);

    PyreProfile before_profile = *p;
    Measured m;
    Score before = {0};
    if (o.verify) {
        for (int t = 1; t < PYRE_TARGET_COUNT; t++) if (o.targets[t]) pyre_target_init(t);
        if (!o.quiet && !json_mode) printf("self-check before training (held-out sizes)...\n");
        if (json_mode) { jbegin("phase"); jkey("name"); jstr("check-before"); jend(); }
        measure(&o, 0.0, &m);
        before = score_measured(&m, "before", 0, NULL, NULL);
        print_score("before", &before);
    }
    double started = pyre_now_seconds();
    int probes = 0;
    if (json_mode) { jbegin("phase"); jkey("name"); jstr("training"); jend(); }
    train_grid(&o, p, &probes);
    double seconds = pyre_now_seconds() - started;
    if (!json_mode) printf("trained %d probes in %.1fs\n", probes, seconds);

    const char *verdict = "unverified";
    int discarded = 0;
    Score after = {0};
    if (o.verify) {
        if (!json_mode) printf("self-check after training (held-out sizes):\n");
        else { jbegin("phase"); jkey("name"); jstr("check-after"); jend(); }
        after = score_measured(&m, "after", 0, NULL, NULL);  /* same measured times: only the profile changed */
        if (!json_mode) { print_score("before", &before); print_score("after", &after); }
        else print_score("after", &after);
        double rb = mean_regret(&before), ra = mean_regret(&after);
        if (!before.shapes || !after.shapes) {
            verdict = "unknown";
            if (!json_mode) printf("verdict: not enough to compare; keeping the training\n");
        } else if (ra > rb + 0.02) {
            verdict = "worse";
            discarded = 1;
            *p = before_profile;  /* training made decisions worse: undo it */
            if (!json_mode) printf("verdict: WORSE (%.1f%% -> %.1f%% time lost). Training discarded; previous knowledge kept.\n", rb * 100, ra * 100);
        } else if (ra < rb - 0.02) {
            verdict = "better";
            if (!json_mode) printf("verdict: BETTER (%.1f%% -> %.1f%% of time lost to wrong placement)\n", rb * 100, ra * 100);
        } else {
            verdict = "same";
            if (!json_mode) printf("verdict: no measurable change (%.1f%% -> %.1f%%); decisions were already as good as these probes can make them\n", rb * 100, ra * 100);
        }
    }
    int saved = discarded ? 0 : pyre_manager_save();
    if (json_mode) {
        jbegin("verdict"); jkey("verdict"); jstr(verdict); jkey("discarded"); printf("%s", discarded ? "true" : "false"); jend();
        jbegin("done"); jkey("probes"); printf("%d", probes); jkey("seconds"); printf("%.2f", seconds);
        jkey("saved"); printf("%s", saved ? "true" : "false"); jend();
    } else if (!discarded) {
        printf("%s %s\n", saved ? "saved to" : "NOT saved (no writable profile path):", pyre_manager_profile_path());
    }
    append_history("train", &o, o.verify ? &before : NULL, o.verify ? &after : NULL, verdict, probes, seconds);
    return discarded ? 1 : 0;
}

int cmd_check(void) {
    TrainOpts o = {.max_probe = 1.0, .max_exp = 30, .gpu_load = 100, .cpu_load = 100, .level = "check"};
    for (int i = 0; i < PYRE_OP_COUNT; i++) o.ops[i] = 1;
    for (int i = 0; i < PYRE_TARGET_COUNT; i++) { o.targets[i] = 1; pyre_target_init(i); }
    if (!json_mode) printf("self-check on held-out sizes (runs every target for real):\n");
    else { jbegin("phase"); jkey("name"); jstr("check"); jend(); }
    double t0 = pyre_now_seconds();
    Measured m;
    measure(&o, 0.0, &m);
    Score s = score_measured(&m, "check", 1, NULL, NULL);
    print_score("now", &s);
    if (json_mode) { jbegin("done"); jkey("probes"); printf("0"); jkey("seconds"); printf("%.2f", pyre_now_seconds() - t0); jkey("saved"); printf("false"); jend(); }
    append_history("check", &o, NULL, &s, "check", 0, pyre_now_seconds() - t0);
    return 0;
}
