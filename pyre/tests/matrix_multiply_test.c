#include "H/runtime/ash_gpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Small enough to compute a trustworthy reference in plain scalar code
 * right here, instead of trusting the optimized kernel to check itself. */
static void naive_multiply(const double *a, const double *b, double *c, int n) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            double sum = 0.0;
            for (int k = 0; k < n; k++) sum += a[i * n + k] * b[k * n + j];
            c[i * n + j] = sum;
        }
    }
}

int main(void) {
    const int n = 37; /* deliberately not a round number */
    double *a = malloc((size_t)n * n * sizeof(double));
    double *b = malloc((size_t)n * n * sizeof(double));
    double *c = malloc((size_t)n * n * sizeof(double));
    double *expected = malloc((size_t)n * n * sizeof(double));

    for (int i = 0; i < n * n; i++) {
        a[i] = (double)((i % 13) - 6) * 0.5;
        b[i] = (double)((i % 7) - 3) * 1.5;
    }

    naive_multiply(a, b, expected, n);
    ash_gpu_matrix_multiply(a, b, c, n);

    int failures = 0;
    for (int i = 0; i < n * n; i++) {
        if (fabs(c[i] - expected[i]) > 1e-9 * (fabs(expected[i]) + 1.0)) {
            if (failures < 5) {
                fprintf(stderr, "mismatch at %d: got %.17g, expected %.17g\n", i, c[i], expected[i]);
            }
            failures++;
        }
    }

    free(a);
    free(b);
    free(c);
    free(expected);

    if (failures) {
        fprintf(stderr, "matrix_multiply_test: FAILED (%d mismatches)\n", failures);
        return 1;
    }
    printf("matrix_multiply_test: OK\n");
    return 0;
}
