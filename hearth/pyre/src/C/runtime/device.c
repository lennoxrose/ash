#define _POSIX_C_SOURCE 200809L
#include "H/runtime/ash_gpu.h"
#include "H/runtime/manager.h"
#include <string.h>

/*
 * Real detection: tries to actually bring up a device, best first -- CUDA
 * (NVIDIA only, present only when pyre/cuda.mk found nvcc at build time),
 * then Vulkan (any vendor), else the CPU fallback. Not a loader-presence
 * check: each init really creates a device. This reports what EXISTS; which
 * of them a given call actually uses is the manager's decision
 * (src/C/runtime/manager.c), per call, from measured cost.
 */
AshGpuInfo ash_gpu_detect(void) {
    AshGpuInfo info;
    memset(&info, 0, sizeof(info));
    info.backend = ASH_GPU_BACKEND_NONE;
    strncpy(info.device_name, "CPU fallback", sizeof(info.device_name) - 1);

    if (pyre_target_init(PYRE_CUDA)) {
        info.backend = ASH_GPU_BACKEND_CUDA;
        info.vram_bytes = pyre_target_vram(PYRE_CUDA);
        strncpy(info.device_name, pyre_target_device(PYRE_CUDA), sizeof(info.device_name) - 1);
    } else if (pyre_target_init(PYRE_VULKAN)) {
        info.backend = ASH_GPU_BACKEND_VULKAN;
        strncpy(info.device_name, pyre_target_device(PYRE_VULKAN), sizeof(info.device_name) - 1);
    }
    info.device_name[sizeof(info.device_name) - 1] = '\0';
    return info;
}

int ash_gpu_available(void) {
    return ash_gpu_detect().backend != ASH_GPU_BACKEND_NONE;
}

// ---- placement queries and the three kernels: describe the work, let the
// manager place it ----

static AshGpuBackend backend_of(PyreTarget t) {
    return t == PYRE_CUDA ? ASH_GPU_BACKEND_CUDA : t == PYRE_VULKAN ? ASH_GPU_BACKEND_VULKAN : ASH_GPU_BACKEND_NONE;
}

static AshGpuPlan plan_for(PyreOp op, PyreWorkload w) {
    double est[PYRE_TARGET_COUNT];
    PyreTarget t = pyre_manager_estimate(op, w, est);
    AshGpuPlan plan = {backend_of(t), est[t], est[PYRE_CPU]};
    return plan;
}

AshGpuPlan ash_gpu_plan_matrix_multiply(int n) { return plan_for(PYRE_OP_MATMUL, pyre_workload_matmul(n)); }
AshGpuPlan ash_gpu_plan_chaos_iterate(int n, int iterations) { return plan_for(PYRE_OP_CHAOS, pyre_workload_chaos(n, iterations)); }
AshGpuPlan ash_gpu_plan_monte_carlo_risk(long long iterations) { return plan_for(PYRE_OP_MONTE_CARLO, pyre_workload_monte_carlo(iterations)); }

typedef struct { const double *a, *b; double *c; int n; } MatmulArgs;
static int run_matmul(PyreTarget t, void *p) {
    MatmulArgs *x = p;
    return pyre_run_matmul(t, x->a, x->b, x->c, x->n);
}
void ash_gpu_matrix_multiply(const double *a, const double *b, double *c, int n) {
    MatmulArgs args = {a, b, c, n};
    pyre_manager_execute(PYRE_OP_MATMUL, pyre_workload_matmul(n), run_matmul, &args);
}

typedef struct { double *data; int n, iterations; } ChaosArgs;
static int run_chaos(PyreTarget t, void *p) {
    ChaosArgs *x = p;
    return pyre_run_chaos(t, x->data, x->n, x->iterations);
}
void ash_gpu_chaos_iterate(double *data, int n, int iterations) {
    ChaosArgs args = {data, n, iterations};
    pyre_manager_execute(PYRE_OP_CHAOS, pyre_workload_chaos(n, iterations), run_chaos, &args);
}

typedef struct {
    long long iterations; double threshold; unsigned int seed;
    long long *risky; double *probability;
} MonteCarloArgs;
static int run_monte_carlo(PyreTarget t, void *p) {
    MonteCarloArgs *x = p;
    return pyre_run_monte_carlo(t, x->iterations, x->threshold, x->seed, x->risky, x->probability);
}
void ash_gpu_monte_carlo_risk(long long iterations, double threshold, unsigned int seed,
                               long long *out_risky, double *out_probability) {
    MonteCarloArgs args = {iterations, threshold, seed, out_risky, out_probability};
    pyre_manager_execute(PYRE_OP_MONTE_CARLO, pyre_workload_monte_carlo(iterations), run_monte_carlo, &args);
}
