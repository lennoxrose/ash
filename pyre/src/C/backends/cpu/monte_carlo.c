#include "H/runtime/ash_gpu.h"
#ifdef _OPENMP
#include <omp.h>
#endif

/*
 * CPU fallback for ash_gpu_monte_carlo_risk. Each iteration of the
 * reference (pyre/benchmarks/monte_carlo_no_pi/mcnp.cpp) is two draws of
 * the same LCG (seed = seed*1664525 + 1013904223) feeding one
 * independent comparison -- no data dependency between iterations
 * except the seed chain itself, so splitting the iteration range across
 * threads just needs each thread to start from its own, well-separated
 * seed instead of continuing one shared chain.
 *
 * splitmix32 (a cheap, well-mixing finalizer, not the LCG above) turns
 * the caller's single seed plus a thread index into that many
 * decorrelated per-thread starting seeds.
 */
static unsigned int splitmix32(unsigned int x) {
    x += 0x9E3779B9u;
    x = (x ^ (x >> 16)) * 0x85EBCA6Bu;
    x = (x ^ (x >> 13)) * 0xC2B2AE35u;
    return x ^ (x >> 16);
}

void ash_gpu_monte_carlo_risk(long long iterations, double threshold, unsigned int seed,
                               long long *out_risky, double *out_probability) {
    long long risky = 0;

#pragma omp parallel
    {
        int tid =
#ifdef _OPENMP
            omp_get_thread_num();
#else
            0;
#endif
        unsigned int local_seed = splitmix32(seed + (unsigned int)tid * 2654435761u);
        long long local_risky = 0;

#pragma omp for schedule(static)
        for (long long i = 0; i < iterations; i++) {
            local_seed = local_seed * 1664525u + 1013904223u;
            double normalized = (double)local_seed / 4294967296.0;

            local_seed = local_seed * 1664525u + 1013904223u;
            double normalized2 = (double)local_seed / 4294967296.0;

            double score = normalized * normalized2;
            if (score > threshold) local_risky++;
        }

#pragma omp atomic
        risky += local_risky;
    }

    *out_risky = risky;
    *out_probability = (double)risky / (double)iterations;
}
