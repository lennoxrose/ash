#define _POSIX_C_SOURCE 200809L
#include "H/runtime/manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Policy constants. These are about the SHAPE of a workload, not about any
// machine: a job under HEAVY_WORK units (1e8 multiply-adds, element
// iterations, ...) finishes in a fraction of a second on any current CPU, so
// it is not worth waking a GPU for; a job whose CPU time is under
// EXPLORE_MIN_S is too cheap to be worth an experiment on an unmeasured size.
#define HEAVY_WORK 1e8
#define WARM_AFTER_S 0.25
#define EXPLORE_MIN_S 0.05

static PyreProfile profile;
static int loaded;
static int dirty;
static int atexit_registered;
static int disabled[PYRE_TARGET_COUNT];
static double cpu_seconds_used;  // total CPU time Pyre has spent in this process
static int trace = -1;
static char path[1024];
static int path_resolved;

PyreWorkload pyre_workload_matmul(int n) { return (PyreWorkload){3.0 * n * n * 8.0, (double)n * n * n}; }
PyreWorkload pyre_workload_chaos(int n, int iterations) { return (PyreWorkload){2.0 * n * 8.0, (double)n * iterations}; }
PyreWorkload pyre_workload_monte_carlo(long long iterations) { return (PyreWorkload){0.0, (double)iterations}; }

// $PYRE_PROFILE, else $XDG_CACHE_HOME/pyre/profile.txt, else
// ~/.cache/pyre/profile.txt. Empty = no home, learn in memory only.
static void resolve_path(void) {
    if (path_resolved) return;
    path_resolved = 1;
    const char *override = getenv("PYRE_PROFILE");
    if (override && *override) { snprintf(path, sizeof(path), "%s", override); return; }
    const char *xdg = getenv("XDG_CACHE_HOME");
    const char *home = getenv("HOME");
    char dir[900];
    if (xdg && *xdg) snprintf(dir, sizeof(dir), "%s", xdg);
    else if (home && *home) snprintf(dir, sizeof(dir), "%s/.cache", home);
    else return;
    mkdir(dir, 0755);
    char sub[960];
    snprintf(sub, sizeof(sub), "%s/pyre", dir);
    mkdir(sub, 0755);
    snprintf(path, sizeof(path), "%s/profile.txt", sub);
}

int pyre_manager_save(void) {
    resolve_path();
    if (!path[0] || !pyre_profile_save(&profile, path)) return 0;
    dirty = 0;
    return 1;
}

static void save_at_exit(void) {
    if (dirty) pyre_manager_save();
}

static void note_dirty(void) {
    dirty = 1;
    if (!atexit_registered) { atexit_registered = 1; atexit(save_at_exit); }
}

static int tracing(void) {
    if (trace < 0) { const char *e = getenv("PYRE_TRACE"); trace = e && *e && *e != '0'; }
    return trace;
}

static void reset_profile(void) {
    memset(&profile, 0, sizeof(profile));
    profile.cpu_threads = pyre_cpu_threads();
    profile.cuda_built = pyre_cuda_built();
    memset(disabled, 0, sizeof(disabled));
}

static void ensure_loaded(void) {
    if (loaded) return;
    loaded = 1;
    resolve_path();
    if (!(path[0] && pyre_profile_load(&profile, path)) ||
        profile.cpu_threads != pyre_cpu_threads() || profile.cuda_built != pyre_cuda_built()) {
        reset_profile();  // other machine / other build / nothing yet: start empty
    }
}

const char *pyre_manager_profile_path(void) { resolve_path(); return path; }
const PyreProfile *pyre_manager_profile(void) { ensure_loaded(); return &profile; }
PyreProfile *pyre_manager_profile_edit(void) { ensure_loaded(); note_dirty(); return &profile; }

void pyre_manager_forget(void) {
    resolve_path();
    loaded = 1;
    reset_profile();
    if (path[0]) remove(path);
    dirty = 0;
}

// Once a GPU target is up, make sure the profile's idea of it is this
// device: a different GPU (or driver reinstall onto other hardware) than
// the one the saved rates describe means those rates are not ours.
static void sync_target(PyreTarget t) {
    if (t == PYRE_CPU || !pyre_target_is_up(t)) return;
    const char *dev = pyre_target_device(t);
    if (profile.present[t] && strcmp(profile.device[t], dev) == 0) return;
    for (int o = 0; o < PYRE_OP_COUNT; o++) memset(&profile.rates[o][t], 0, sizeof(PyreRates));
    profile.present[t] = 1;
    snprintf(profile.device[t], sizeof(profile.device[t]), "%s", dev);
    profile.init_seconds[t] = pyre_target_init_seconds(t);
    note_dirty();
}

typedef enum { SRC_NONE, SRC_KNOWN, SRC_INFERRED } Source;

static PyreTarget decide(PyreOp op, PyreWorkload w, double est[PYRE_TARGET_COUNT], Source src[PYRE_TARGET_COUNT]) {
    double cpu = pyre_rates_nearest(&profile.rates[op][PYRE_CPU], w.work);
    est[PYRE_CPU] = cpu;
    src[PYRE_CPU] = cpu >= 0 ? SRC_KNOWN : SRC_NONE;

    for (int t = PYRE_CPU + 1; t < PYRE_TARGET_COUNT; t++) {
        est[t] = -1.0;
        src[t] = SRC_NONE;
        if (disabled[t] || pyre_target_failed(t) || !pyre_target_supports(t, op)) continue;
        sync_target(t);
        double known = pyre_rates_known(&profile.rates[op][t], w.work);
        if (known >= 0) { est[t] = known; src[t] = SRC_KNOWN; continue; }
        // A rate measured at a smaller size is pessimistic here (fixed costs
        // amortise as work grows): if even that beats the CPU, the GPU wins.
        double below = pyre_rates_from_below(&profile.rates[op][t], w.work);
        if (below >= 0 && cpu >= 0 && below < cpu) { est[t] = below; src[t] = SRC_INFERRED; }
    }

    if (cpu < 0) return PYRE_CPU;  // nothing learned about the CPU yet: just run it

    PyreTarget best = PYRE_CPU;
    double best_cost = cpu;
    for (int t = PYRE_CPU + 1; t < PYRE_TARGET_COUNT; t++) {
        if (est[t] < 0) continue;
        double cost = est[t];
        if (!pyre_target_is_up(t)) {
            if (pyre_target_warming()) continue;  // never wait for a start-up in flight
            // Starting it in the foreground only when we already KNOW (from a
            // previous start on this machine) that the saving beats the start.
            if (profile.init_seconds[t] <= 0 || cpu - est[t] <= profile.init_seconds[t]) continue;
            cost += profile.init_seconds[t];
        }
        if (cost < best_cost) { best = t; best_cost = cost; }
    }
    if (best != PYRE_CPU) return best;

    // Nothing measured beats the CPU. If the job is big enough to make one
    // experiment cheap, try an unmeasured size on a GPU that is already up.
    if (cpu >= EXPLORE_MIN_S) {
        for (int t = PYRE_CPU + 1; t < PYRE_TARGET_COUNT; t++) {
            if (!disabled[t] && pyre_target_is_up(t) && pyre_target_supports(t, op) &&
                pyre_rates_known(&profile.rates[op][t], w.work) < 0) {
                return t;
            }
        }
    }
    return PYRE_CPU;
}

PyreTarget pyre_manager_estimate(PyreOp op, PyreWorkload w, double seconds[PYRE_TARGET_COUNT]) {
    ensure_loaded();
    Source src[PYRE_TARGET_COUNT];
    return decide(op, w, seconds, src);
}

static void maybe_warm_gpu(PyreWorkload w) {
    if (pyre_target_warming()) return;
    for (int t = PYRE_CPU + 1; t < PYRE_TARGET_COUNT; t++) if (pyre_target_is_up(t)) return;
    if (w.work >= HEAVY_WORK || cpu_seconds_used >= WARM_AFTER_S) pyre_target_warm_async();
}

void pyre_manager_execute(PyreOp op, PyreWorkload w, PyreRunFn run, void *args) {
    ensure_loaded();
    for (int attempt = 0; attempt < PYRE_TARGET_COUNT + 2; attempt++) {
        double est[PYRE_TARGET_COUNT];
        Source src[PYRE_TARGET_COUNT];
        PyreTarget t = decide(op, w, est, src);

        if (!pyre_target_is_up(t)) {
            // Only reached when the profile already proved the start-up pays.
            if (!pyre_target_init(t)) { disabled[t] = 1; continue; }
            sync_target(t);
            continue;  // re-decide: a different device voids the old rates
        }
        if (t == PYRE_CPU) maybe_warm_gpu(w);

        double t0 = pyre_now_seconds();
        int ok = run(t, args);
        double dt = pyre_now_seconds() - t0;
        if (!ok) { disabled[t] = 1; continue; }

        if (t == PYRE_CPU) cpu_seconds_used += dt;
        pyre_rates_observe(&profile.rates[op][t], w.work, dt);
        note_dirty();

        if (tracing()) {
            fprintf(stderr, "pyre: %s work=%.3g -> %s %.4gs  [", pyre_op_name(op), w.work, pyre_target_name(t), dt);
            for (int o = 0; o < PYRE_TARGET_COUNT; o++) {
                if (o) fputs(", ", stderr);
                if (est[o] >= 0) fprintf(stderr, "%s est %.4gs%s", pyre_target_name(o), est[o], src[o] == SRC_INFERRED ? " (inferred)" : "");
                else fprintf(stderr, "%s %s", pyre_target_name(o),
                             pyre_target_warming() && o != PYRE_CPU ? "starting" : "unmeasured");
            }
            fputs("]\n", stderr);
        }
        return;
    }
    run(PYRE_CPU, args);
}
