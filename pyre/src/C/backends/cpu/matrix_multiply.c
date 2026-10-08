#include "H/backends/cpu/matrix_multiply.h"
#include <string.h>

/*
 * CPU backend for matrix_multiply -- always available (no hardware
 * needed), so the dispatcher in src/C/runtime/device.c falls back to
 * this whenever ash_gpu_vulkan_init() hasn't found a usable device
 * (which, on this sandbox, is always -- see context.cpp).
 *
 * Two changes from the naive ijk reference (pyre/benchmarks/matrix/matrix_multiply.cpp):
 *
 * 1. Loop order ikj instead of ijk. The naive order's innermost loop
 *    reads B[k][j] for fixed i,j while k varies -- a stride of N
 *    doubles through a 32MB matrix, missing cache on nearly every
 *    access. Swapping to ikj makes the innermost loop walk j across a
 *    single row of B and a single row of C, both sequential, which also
 *    lets the compiler auto-vectorize it (contiguous FMA over doubles).
 * 2. OpenMP across the outer i loop -- each output row is independent,
 *    so this is an embarrassingly parallel split across cores with no
 *    locking needed.
 *
 * No GPU required for either change; this is squarely the "best CPU
 * implementation" design principle 1 in ideas/gpu_acceleration.md asks
 * for when there is no usable device.
 */
void ash_gpu_cpu_matrix_multiply(const double *a, const double *b, double *c, int n) {
    memset(c, 0, (size_t)n * (size_t)n * sizeof(double));

#pragma omp parallel for schedule(static)
    for (int i = 0; i < n; i++) {
        const double *a_row = a + (size_t)i * n;
        double *c_row = c + (size_t)i * n;
        for (int k = 0; k < n; k++) {
            double a_ik = a_row[k];
            const double *b_row = b + (size_t)k * n;
#pragma omp simd
            for (int j = 0; j < n; j++) {
                c_row[j] += a_ik * b_row[j];
            }
        }
    }
}
