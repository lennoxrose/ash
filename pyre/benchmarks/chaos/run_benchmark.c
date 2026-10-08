/* Not one of the user's two original challenges (matrix_multiply,
 * monte_carlo_no_pi) -- added after matrix_multiply's real GPU
 * measurement came back a loss (see ash_gpu.h's comment on that
 * function), to answer "is there ANY workload shape where this GPU
 * setup (RX 9070 XT via WSL2's Dozen/D3D12 translation layer) actually
 * wins?" Shaped deliberately opposite to matrix_multiply: huge compute
 * per element, trivial data movement, no tiling to get right -- see
 * ash_gpu_chaos_iterate's own doc comment.
 *
 * Calls the CPU and Vulkan backends directly (bypassing
 * ash_gpu_chaos_iterate's own dispatcher) so both run back to back in
 * one process under identical conditions, instead of needing a
 * separate run plus an env-var trick to force one path. */
#include "H/runtime/ash_gpu.h"
#include "H/backends/cpu/chaos.h"
#include "H/backends/vulkan/context.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(void) {
    const int n = 2000000;
    const int iterations = 4000;

    double *data_cpu = malloc((size_t)n * sizeof(double));
    double *data_gpu = malloc((size_t)n * sizeof(double));
    for (int i = 0; i < n; i++) {
        double v = (double)(i % 1000) * 0.001;
        data_cpu[i] = v;
        data_gpu[i] = v;
    }

    printf("chaos_iterate: n=%d, iterations=%d (%.1f billion ops)\n",
           n, iterations, (double)n * iterations / 1e9);

    double t0 = now_seconds();
    ash_gpu_cpu_chaos_iterate(data_cpu, n, iterations);
    double cpu_seconds = now_seconds() - t0;
    printf("CPU backend:  %.3fs\n", cpu_seconds);

    AshGpuInfo info = ash_gpu_detect();
    if (info.backend == ASH_GPU_BACKEND_NONE) {
        printf("GPU backend:  unavailable (%s)\n", info.device_name);
        free(data_cpu);
        free(data_gpu);
        return 0;
    }

    t0 = now_seconds();
    ash_gpu_vulkan_chaos_iterate(data_gpu, n, iterations);
    double gpu_seconds = now_seconds() - t0;
    printf("GPU backend:  %.3fs (%s)\n", gpu_seconds, info.device_name);
    printf("speedup: %.2fx\n", cpu_seconds / gpu_seconds);

    /* Both backends run the identical recurrence -- results must match
     * (within fp rounding-order slack, since the GPU evaluates per
     * thread independently same as the CPU's OpenMP loop does, both
     * single-precision-free, double throughout). */
    double max_diff = 0.0;
    for (int i = 0; i < n; i++) {
        double diff = data_cpu[i] - data_gpu[i];
        if (diff < 0) diff = -diff;
        if (diff > max_diff) max_diff = diff;
    }
    printf("max |CPU - GPU| difference: %g\n", max_diff);

    free(data_cpu);
    free(data_gpu);
    return 0;
}
