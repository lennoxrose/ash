#define _POSIX_C_SOURCE 200809L
#include "H/runtime/targets.h"
#include "H/backends/cpu/matrix_multiply.h"
#include "H/backends/cpu/chaos.h"
#include "H/backends/cpu/monte_carlo.h"
#include "H/backends/vulkan/context.h"
#ifdef ASH_GPU_HAVE_CUDA
#include "H/backends/cuda/context.h"
#endif
#ifdef _OPENMP
#include <omp.h>
#endif
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

// up[] is read lock-free by the manager while the warm-up thread may be
// writing it; init itself is serialised by init_lock.
static atomic_int up[PYRE_TARGET_COUNT];
static atomic_int tried[PYRE_TARGET_COUNT];
static double init_seconds[PYRE_TARGET_COUNT];
static pthread_mutex_t init_lock = PTHREAD_MUTEX_INITIALIZER;
static atomic_int warming;
static atomic_int warm_started;

const char *pyre_op_name(PyreOp op) {
    static const char *names[PYRE_OP_COUNT] = {"matrix_multiply", "chaos_iterate", "monte_carlo_risk"};
    return names[op];
}

const char *pyre_target_name(PyreTarget t) {
    static const char *names[PYRE_TARGET_COUNT] = {"cpu", "cuda", "vulkan"};
    return names[t];
}

int pyre_cpu_threads(void) {
#ifdef _OPENMP
    return omp_get_max_threads();
#else
    return 1;
#endif
}

int pyre_cuda_built(void) {
#ifdef ASH_GPU_HAVE_CUDA
    return 1;
#else
    return 0;
#endif
}

int pyre_target_supports(PyreTarget t, PyreOp op) {
    switch (t) {
    case PYRE_CPU: return 1;
#ifdef ASH_GPU_HAVE_CUDA
    case PYRE_CUDA: return 1;
#endif
    case PYRE_VULKAN: return op != PYRE_OP_MONTE_CARLO;
    default: return 0;
    }
}

int pyre_target_init(PyreTarget t) {
    if (t == PYRE_CPU) return 1;
    if (atomic_load(&tried[t])) return atomic_load(&up[t]);
    pthread_mutex_lock(&init_lock);
    if (!atomic_load(&tried[t])) {
        double t0 = pyre_now_seconds();
        int ok = 0;
        if (t == PYRE_VULKAN) ok = ash_gpu_vulkan_init();
#ifdef ASH_GPU_HAVE_CUDA
        else if (t == PYRE_CUDA) ok = ash_gpu_cuda_init();
#endif
        init_seconds[t] = ok ? pyre_now_seconds() - t0 : 0.0;
        atomic_store(&up[t], ok);
        atomic_store(&tried[t], 1);
    }
    pthread_mutex_unlock(&init_lock);
    return atomic_load(&up[t]);
}

int pyre_target_is_up(PyreTarget t) { return t == PYRE_CPU || atomic_load(&up[t]); }
int pyre_target_failed(PyreTarget t) { return t != PYRE_CPU && atomic_load(&tried[t]) && !atomic_load(&up[t]); }
double pyre_target_init_seconds(PyreTarget t) { return init_seconds[t]; }

const char *pyre_target_device(PyreTarget t) {
    static char cpu[64];
    switch (t) {
    case PYRE_CPU:
        snprintf(cpu, sizeof(cpu), "CPU (%d threads)", pyre_cpu_threads());
        return cpu;
#ifdef ASH_GPU_HAVE_CUDA
    case PYRE_CUDA: return up[t] ? ash_gpu_cuda_device_name() : "";
#endif
    case PYRE_VULKAN: return up[t] ? ash_gpu_vulkan_device_name() : "";
    default: return "";
    }
}

unsigned long long pyre_target_vram(PyreTarget t) {
#ifdef ASH_GPU_HAVE_CUDA
    if (t == PYRE_CUDA && up[t]) return ash_gpu_cuda_vram_bytes();
#endif
    (void)t;
    return 0;
}

int pyre_run_matmul(PyreTarget t, const double *a, const double *b, double *c, int n) {
    switch (t) {
    case PYRE_CPU: ash_gpu_cpu_matrix_multiply(a, b, c, n); return 1;
#ifdef ASH_GPU_HAVE_CUDA
    case PYRE_CUDA: return ash_gpu_cuda_matrix_multiply(a, b, c, n);
#endif
    case PYRE_VULKAN: ash_gpu_vulkan_matrix_multiply(a, b, c, n); return 1;
    default: return 0;
    }
}

int pyre_run_chaos(PyreTarget t, double *data, int n, int iterations) {
    switch (t) {
    case PYRE_CPU: ash_gpu_cpu_chaos_iterate(data, n, iterations); return 1;
#ifdef ASH_GPU_HAVE_CUDA
    case PYRE_CUDA: return ash_gpu_cuda_chaos_iterate(data, n, iterations);
#endif
    case PYRE_VULKAN: ash_gpu_vulkan_chaos_iterate(data, n, iterations); return 1;
    default: return 0;
    }
}

int pyre_run_monte_carlo(PyreTarget t, long long iterations, double threshold, unsigned int seed,
                         long long *out_risky, double *out_probability) {
    switch (t) {
    case PYRE_CPU:
        ash_gpu_cpu_monte_carlo_risk(iterations, threshold, seed, out_risky, out_probability);
        return 1;
#ifdef ASH_GPU_HAVE_CUDA
    case PYRE_CUDA:
        return ash_gpu_cuda_monte_carlo_risk(iterations, threshold, seed, out_risky, out_probability);
#endif
    default: return 0;
    }
}

// One tiny run of every kernel the target has, so lazy loading (CUDA modules,
// Vulkan pipelines) is done before real work arrives.
static void preload(PyreTarget t) {
    double a[16 * 16], c[16 * 16], data[1000];
    for (int i = 0; i < 16 * 16; i++) a[i] = 1.0;
    for (int i = 0; i < 1000; i++) data[i] = 0.5;
    pyre_run_matmul(t, a, a, c, 16);
    pyre_run_chaos(t, data, 1000, 2);
    if (pyre_target_supports(t, PYRE_OP_MONTE_CARLO)) {
        long long risky;
        double p;
        pyre_run_monte_carlo(t, 1000, 0.75, 1u, &risky, &p);
    }
}

static void *warm_thread(void *arg) {
    (void)arg;
    // Best first: CUDA, else Vulkan. A machine with a working CUDA device
    // gains nothing from also starting Vulkan on the same card.
    static const PyreTarget order[] = {PYRE_CUDA, PYRE_VULKAN};
    for (unsigned i = 0; i < sizeof(order) / sizeof(order[0]); i++) {
        if (pyre_target_init(order[i])) { preload(order[i]); break; }
    }
    atomic_store(&warming, 0);
    return NULL;
}

void pyre_target_warm_async(void) {
    if (atomic_exchange(&warm_started, 1)) return;
    atomic_store(&warming, 1);
    pthread_t th;
    if (pthread_create(&th, NULL, warm_thread, NULL) == 0) pthread_detach(th);
    else atomic_store(&warming, 0);
}

int pyre_target_warming(void) { return atomic_load(&warming); }
