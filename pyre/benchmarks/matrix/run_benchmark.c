/* Pyre's side of the matrix_multiply challenge: same N, same values,
 * same "time this with the `time` command" methodology as
 * matrix_multiply.cpp, but through ash_gpu_matrix_multiply (CPU backend:
 * ikj loop order + OpenMP, see src/C/backends/cpu/matrix_multiply.c) instead
 * of the naive ijk double** reference. Goal (matrix_multiply.ash's own
 * comment): beat matrix_multiply.cpp's 5.761s by at least 50%, i.e.
 * finish in under ~2.88s on this machine.
 */
#include "H/runtime/ash_gpu.h"
#include <stdio.h>
#include <stdlib.h>

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

    double expected = (double)n * 1.5 * 2.0; /* every term of the dot product is 1.5*2.0, N of them */
    int ok = c[0] == expected;

    free(a);
    free(b);
    free(c);
    return ok ? 0 : 1;
}
