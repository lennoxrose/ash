#include "H/runtime/ash_gpu.h"
#include <stdio.h>

int main(void) {
    long long risky = 0;
    double probability = 0.0;
    const long long iterations = 10000000;

    ash_gpu_monte_carlo_risk(iterations, 0.75, 123456789u, &risky, &probability);

    int ok = 1;
    /* Self-consistency: the reported probability must be exactly the
     * reported count over the iteration count, regardless of how the
     * threads split the work. */
    double recomputed = (double)risky / (double)iterations;
    if (probability != recomputed) {
        fprintf(stderr, "monte_carlo_test: probability %.17g != risky/iterations %.17g\n",
                probability, recomputed);
        ok = 0;
    }
    /* Loose sanity bound on P(X*Y > 0.75) for independent X,Y ~ Uniform(0,1):
     * analytically 0.75 - 0.75*ln(0.75) ~= 0.0345, so anything wildly off
     * (e.g. every/no draw flagged) means the kernel is broken, not just
     * imprecise. */
    if (probability < 0.01 || probability > 0.08) {
        fprintf(stderr, "monte_carlo_test: probability %.6f outside sane range [0.01, 0.08]\n",
                probability);
        ok = 0;
    }

    if (!ok) {
        fprintf(stderr, "monte_carlo_test: FAILED\n");
        return 1;
    }
    printf("monte_carlo_test: OK (risky=%lld, probability=%.6f)\n", risky, probability);
    return 0;
}
