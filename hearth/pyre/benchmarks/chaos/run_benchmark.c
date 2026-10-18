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
#ifdef ASH_GPU_HAVE_CUDA
#include "H/backends/cuda/context.h"
#endif
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

    int exit_code = 0;

#ifdef ASH_GPU_HAVE_CUDA
    /* CUDA first (NVIDIA only): a warm-up call so context creation and the
     * first kernel's JIT/load aren't charged to the measurement. */
    if (ash_gpu_cuda_init()) {
        double warm = 0.5;
        ash_gpu_cuda_chaos_iterate(&warm, 1, 1);

        double *data_cuda = malloc((size_t)n * sizeof(double));
        for (int i = 0; i < n; i++) data_cuda[i] = (double)(i % 1000) * 0.001;
        t0 = now_seconds();
        int ran = ash_gpu_cuda_chaos_iterate(data_cuda, n, iterations);
        double cuda_seconds = now_seconds() - t0;
        if (ran) {
            double max_diff = 0.0;
            for (int i = 0; i < n; i++) {
                double d = data_cpu[i] - data_cuda[i];
                if (d < 0) d = -d;
                if (d > max_diff) max_diff = d;
            }
            printf("CUDA backend: %.3fs (%s)  speedup: %.2fx  max |CPU - CUDA|: %g\n",
                   cuda_seconds, ash_gpu_cuda_device_name(), cpu_seconds / cuda_seconds, max_diff);
            if (max_diff != 0.0) exit_code = 1;
        } else {
            printf("CUDA backend: launch failed\n");
        }
        free(data_cuda);
    }
#endif

    if (!ash_gpu_vulkan_init()) {
        printf("Vulkan backend: unavailable\n");
        free(data_cpu);
        free(data_gpu);
        return exit_code;
    }

    t0 = now_seconds();
    ash_gpu_vulkan_chaos_iterate(data_gpu, n, iterations);
    double gpu_seconds = now_seconds() - t0;
    printf("Vulkan backend: %.3fs (%s)  speedup: %.2fx\n",
           gpu_seconds, ash_gpu_vulkan_device_name(), cpu_seconds / gpu_seconds);

    double max_diff = 0.0;
    for (int i = 0; i < n; i++) {
        double diff = data_cpu[i] - data_gpu[i];
        if (diff < 0) diff = -diff;
        if (diff > max_diff) max_diff = diff;
    }
    printf("max |CPU - Vulkan| difference: %g\n", max_diff);

    free(data_cpu);
    free(data_gpu);
    return exit_code;
}
