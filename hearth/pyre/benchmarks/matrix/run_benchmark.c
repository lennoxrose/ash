/* Pyre's side of the matrix_multiply challenge: same N, same values,
 * same "time this with the `time` command" methodology as
 * matrix_multiply.cpp, but through ash_gpu_matrix_multiply (CPU backend:
 * ikj loop order + OpenMP, see src/C/backends/cpu/matrix_multiply.c) instead
 * of the naive ijk double** reference. Goal (matrix_multiply.ash's own
 * comment): beat matrix_multiply.cpp's 5.761s by at least 50%, i.e.
 * finish in under ~2.88s on this machine.
 */
#include "H/runtime/ash_gpu.h"
#include "H/backends/cpu/matrix_multiply.h"
#ifdef ASH_GPU_HAVE_CUDA
#include "H/backends/cuda/context.h"
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

#ifdef ASH_GPU_HAVE_CUDA
/* Backend-by-backend timing at the benchmark's own N, on non-constant data
 * (the challenge's constant fill can't catch a wrong index), CUDA checked
 * against the CPU backend's result. */
static void compare_backends(int n) {
    size_t cnt = (size_t)n * n;
    double *a = malloc(cnt * sizeof(double)), *b = malloc(cnt * sizeof(double));
    double *c_cpu = malloc(cnt * sizeof(double)), *c_cuda = malloc(cnt * sizeof(double));
    for (size_t i = 0; i < cnt; i++) {
        a[i] = (double)((i % 13) - 6) * 0.5;
        b[i] = (double)((i % 7) - 3) * 1.5;
    }
    double t0 = now_seconds();
    ash_gpu_cpu_matrix_multiply(a, b, c_cpu, n);
    double cpu_s = now_seconds() - t0;

    if (ash_gpu_cuda_init()) {
        ash_gpu_cuda_matrix_multiply(a, b, c_cuda, 64); /* warm-up: context + kernel load */
        t0 = now_seconds();
        int ran = ash_gpu_cuda_matrix_multiply(a, b, c_cuda, n);
        double cuda_s = now_seconds() - t0;
        double max_err = 0.0;
        for (size_t i = 0; i < cnt; i++) {
            double e = fabs(c_cpu[i] - c_cuda[i]);
            if (e > max_err) max_err = e;
        }
        printf("  CPU  : %.3fs\n  CUDA : %.3fs (%s, %.1f GFLOP/s)  max |CPU - CUDA| = %g%s\n",
               cpu_s, cuda_s, ash_gpu_cuda_device_name(),
               2.0 * n * n * (double)n / cuda_s / 1e9, max_err, ran ? "" : "  [LAUNCH FAILED]");
    } else {
        printf("  CPU  : %.3fs\n  CUDA : unavailable\n", cpu_s);
    }
    free(a); free(b); free(c_cpu); free(c_cuda);
}
#endif

int main(void) {
    const int n = 2000;
    double *a = malloc((size_t)n * n * sizeof(double));
    double *b = malloc((size_t)n * n * sizeof(double));
    double *c = malloc((size_t)n * n * sizeof(double));

    for (int i = 0; i < n * n; i++) {
        a[i] = 1.5;
        b[i] = 2.0;
    }

    printf("Starting matrix multiplication (%dx%d)...\n", n, n);
    ash_gpu_matrix_multiply(a, b, c, n);
    printf("Done!\n");
    printf("Sample element C[0][0]: %g\n", c[0]);
#ifdef ASH_GPU_HAVE_CUDA
    printf("Backends at N=%d:\n", n);
    compare_backends(n);
#endif

    double expected = (double)n * 1.5 * 2.0; /* every term of the dot product is 1.5*2.0, N of them */
    int ok = c[0] == expected;

    free(a);
    free(b);
    free(c);
    return ok ? 0 : 1;
}
