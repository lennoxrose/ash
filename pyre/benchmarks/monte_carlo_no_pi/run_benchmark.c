/* Pyre's side of the monte_carlo_no_pi challenge: same iteration count,
 * threshold and recurrence as mcnp.cpp, through ash_gpu_monte_carlo_risk
 * (CPU backend: OpenMP across independently-seeded per-thread streams,
 * see src/C/backends/cpu/monte_carlo.c). The risky count will differ from a
 * single-threaded run with the same seed (see ash_gpu.h's comment on
 * this kernel) -- the benchmark's target is wall-clock time, measured
 * the same way mcnp.ash's own comment asks for (the `time` command).
 * Goal: beat mcnp.cpp's 1.908s by at least 50%, i.e. finish in under
 * ~0.95s on this machine.
 */
#include "H/runtime/ash_gpu.h"
#include <stdio.h>

int main(void) {
    const long long iterations = 1000000000LL;
    const double threshold = 0.75;
    long long risky = 0;
    double probability = 0.0;

    printf("Starting Monte Carlo simulation...\n");
    printf("Iterations: %lld\n", iterations);

    ash_gpu_monte_carlo_risk(iterations, threshold, 123456789u, &risky, &probability);

    printf("Done!\n");
    printf("Risky outcomes: %lld\n", risky);
    printf("Estimated probability: %g\n", probability);
    return 0;
}
