#ifndef PYRE_COST_MODEL_H
#define PYRE_COST_MODEL_H
// What this machine has actually been seen to do, learned from real calls.
//
// There is no calibration pass and no shipped number: a program compiled on
// one machine and run on another starts knowing nothing and learns THAT
// machine from the work it really does. For every (kernel, target) the model
// is a table indexed by workload size (powers of two of the kernel's work
// units: n^3 for matmul, n*iterations for chaos, iterations for Monte
// Carlo) holding the measured seconds-per-work-unit at that size. Rates are
// kept per size bucket, not as one formula, because both transfer overhead
// and cache effects change the effective rate with size and a table simply
// records what happened.
#include "H/runtime/targets.h"

#define PYRE_BUCKETS 48

typedef struct {
    double rate[PYRE_BUCKETS];          // measured seconds per work unit, EMA
    unsigned char seen[PYRE_BUCKETS];   // 1 once a real call landed in the bucket
} PyreRates;

typedef struct { double bytes, work; } PyreWorkload;  // bytes: informational (trace/plan)

int pyre_work_bucket(double work);

// Fold one real, timed call in (first sample sets the rate, later ones
// move it 30% of the way).
void pyre_rates_observe(PyreRates *r, double work, double seconds);

// Predicted seconds for `work`, only from a bucket measured at exactly this
// size; < 0 if that size was never seen.
double pyre_rates_known(const PyreRates *r, double work);

// Predicted seconds from the nearest measured size at or BELOW this one
// (a rate measured at a smaller size is pessimistic for a larger one,
// since fixed overheads amortise), or < 0 if nothing smaller was seen.
double pyre_rates_from_below(const PyreRates *r, double work);

// Predicted seconds from the nearest measured size on either side, or < 0.
double pyre_rates_nearest(const PyreRates *r, double work);

// Everything learned so far. Persisting it is an optimisation for the
// next run on the SAME machine -- it is never required, never shipped, and
// is discarded when the hardware it describes is not the hardware found.
#define PYRE_PROFILE_VERSION 2
typedef struct {
    int cpu_threads;
    int cuda_built;
    int present[PYRE_TARGET_COUNT];
    char device[PYRE_TARGET_COUNT][128];
    double init_seconds[PYRE_TARGET_COUNT];
    PyreRates rates[PYRE_OP_COUNT][PYRE_TARGET_COUNT];
} PyreProfile;

int pyre_profile_load(PyreProfile *p, const char *path);  // 0 = missing/foreign: start empty
int pyre_profile_save(const PyreProfile *p, const char *path);

#endif
