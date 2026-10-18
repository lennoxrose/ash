/* CUDA backend vs CPU backend on shapes that stress the kernels' edge
 * handling: sizes that aren't multiples of the 64x64x16 matmul tile, odd
 * element counts around the chaos kernel's 1024-element blocks and its
 * multi-stream chunking, and Monte Carlo thresholds at/outside [0,1).
 * Skips (exit 0) when there is no CUDA device or CUDA wasn't built in. */
#include "H/runtime/ash_gpu.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef ASH_GPU_HAVE_CUDA
int main(void) { printf("cuda_test: SKIPPED (built without CUDA)\n"); return 0; }
#else
#include "H/backends/cpu/matrix_multiply.h"
#include "H/backends/cpu/chaos.h"
#include "H/backends/cpu/monte_carlo.h"
#include "H/backends/cuda/context.h"

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); failures++; } } while (0)

static void check_matmul(int n) {
    size_t cnt = (size_t)n * n;
    double *a = malloc(cnt * 8), *b = malloc(cnt * 8), *want = malloc(cnt * 8), *got = malloc(cnt * 8);
    for (size_t i = 0; i < cnt; i++) {
        a[i] = (double)((i * 7919) % 101) / 17.0 - 3.0;
        b[i] = (double)((i * 104729) % 89) / 13.0 - 2.5;
    }
    ash_gpu_cpu_matrix_multiply(a, b, want, n);
    CHECK(ash_gpu_cuda_matrix_multiply(a, b, got, n) == 1, "matmul n=%d: CUDA reported failure", n);
    for (size_t i = 0; i < cnt; i++) {
        if (fabs(got[i] - want[i]) > 1e-9 * (fabs(want[i]) + 1.0)) {
            CHECK(0, "matmul n=%d: element %zu got %.17g want %.17g", n, i, got[i], want[i]);
            break;
        }
    }
    free(a); free(b); free(want); free(got);
}

static void check_chaos(int n, int iterations) {
    double *want = malloc((size_t)n * 8), *got = malloc((size_t)n * 8);
    for (int i = 0; i < n; i++) want[i] = got[i] = (double)(i % 1000) * 0.001 - 0.2;
    ash_gpu_cpu_chaos_iterate(want, n, iterations);
    CHECK(ash_gpu_cuda_chaos_iterate(got, n, iterations) == 1, "chaos n=%d: CUDA reported failure", n);
    for (int i = 0; i < n; i++) {
        if (got[i] != want[i]) { CHECK(0, "chaos n=%d: element %d got %.17g want %.17g", n, i, got[i], want[i]); break; }
    }
    free(want); free(got);
}

static void check_monte_carlo(double threshold, long long iterations) {
    long long risky; double p;
    CHECK(ash_gpu_cuda_monte_carlo_risk(iterations, threshold, 42u, &risky, &p) == 1,
          "monte_carlo thr=%g: CUDA reported failure", threshold);
    if (threshold >= 1.0) CHECK(risky == 0, "monte_carlo thr=%g: expected 0, got %lld", threshold, risky);
    else if (threshold < 0.0) CHECK(risky == iterations, "monte_carlo thr=%g: expected all, got %lld", threshold, risky);
    else {
        /* P(U1*U2 > t) = 1 - t + t*ln(t) for independent uniforms. */
        double expect = threshold == 0.0 ? 1.0 : 1.0 - threshold + threshold * log(threshold);
        CHECK(fabs(p - expect) < 0.002, "monte_carlo thr=%g: p=%g, analytic %g", threshold, p, expect);
    }
}

int main(void) {
    if (!ash_gpu_cuda_init()) { printf("cuda_test: SKIPPED (no CUDA device)\n"); return 0; }

    static const int sizes[] = {1, 2, 7, 15, 16, 17, 63, 64, 65, 100, 129, 511, 513};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) check_matmul(sizes[i]);

    static const int counts[] = {1, 3, 1023, 1024, 1025, 262143, 262145, 1000003};
    for (size_t i = 0; i < sizeof(counts) / sizeof(counts[0]); i++) check_chaos(counts[i], 50);

    static const double thresholds[] = {-0.5, 0.0, 0.1, 0.75, 0.999, 1.0, 2.0};
    for (size_t i = 0; i < sizeof(thresholds) / sizeof(thresholds[0]); i++) check_monte_carlo(thresholds[i], 20000000LL);
    { /* fewer iterations than GPU threads: just must run and stay in range */
        long long risky; double p;
        CHECK(ash_gpu_cuda_monte_carlo_risk(1, 0.5, 42u, &risky, &p) == 1 && (risky == 0 || risky == 1),
              "monte_carlo with 1 iteration: bad result %lld", risky);
    }

    if (failures) { fprintf(stderr, "cuda_test: FAILED (%d)\n", failures); return 1; }
    printf("cuda_test: OK (%s)\n", ash_gpu_cuda_device_name());
    return 0;
}
#endif
