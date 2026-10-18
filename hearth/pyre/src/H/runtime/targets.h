#ifndef PYRE_TARGETS_H
#define PYRE_TARGETS_H
// Internal to the runtime (device.c, manager.c, calibrate.c, tools/pyre.c):
// one uniform way to bring up and run each place a kernel can execute, so
// the manager can reason about "CPU / CUDA / Vulkan" without knowing any
// backend's API. Not part of ash_gpu.h's public ABI.
#include <time.h>

typedef enum { PYRE_OP_MATMUL, PYRE_OP_CHAOS, PYRE_OP_MONTE_CARLO, PYRE_OP_COUNT } PyreOp;
typedef enum { PYRE_CPU, PYRE_CUDA, PYRE_VULKAN, PYRE_TARGET_COUNT } PyreTarget;

const char *pyre_op_name(PyreOp op);
const char *pyre_target_name(PyreTarget t);

// The two build/machine facts a measured profile is only valid for.
int pyre_cpu_threads(void);
int pyre_cuda_built(void);

// Whether this target can run this op at all (Vulkan has no Monte Carlo
// kernel; CUDA is absent unless pyre/cuda.mk found nvcc at build time).
int pyre_target_supports(PyreTarget t, PyreOp op);

// Brings the target up (idempotent, remembers the answer). 1 = usable.
// The first successful call is timed: pyre_target_init_seconds() is the
// one-off price of using that target in a fresh process, which the
// manager charges to the first job that goes there.
int pyre_target_init(PyreTarget t);
int pyre_target_is_up(PyreTarget t);          // already initialised and usable (never blocks)
int pyre_target_failed(PyreTarget t);         // init was attempted and did not work
double pyre_target_init_seconds(PyreTarget t);
const char *pyre_target_device(PyreTarget t);
unsigned long long pyre_target_vram(PyreTarget t);

// Run a kernel on a target. 1 = it ran, 0 = it could not (caller falls
// back). CPU always runs.
// Starts a background thread that brings up the best available GPU
// (CUDA, else Vulkan) and runs one tiny kernel of each kind on it so lazy
// module / pipeline loading is paid there, not by a real call. Returns at
// once; at most one warm-up ever runs. A call never waits for it -- the
// manager just keeps using the CPU until pyre_target_is_up() turns true.
void pyre_target_warm_async(void);
int pyre_target_warming(void);

int pyre_run_matmul(PyreTarget t, const double *a, const double *b, double *c, int n);
int pyre_run_chaos(PyreTarget t, double *data, int n, int iterations);
int pyre_run_monte_carlo(PyreTarget t, long long iterations, double threshold, unsigned int seed,
                         long long *out_risky, double *out_probability);

static inline double pyre_now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

#endif
