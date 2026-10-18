// CUDA kernel for ash_gpu_monte_carlo_risk (ash_gpu.h): two LCG draws per
// iteration, count how often their product exceeds `threshold`.
//
// Same recurrence and per-thread seed derivation as the CPU backend
// (src/C/backends/cpu/monte_carlo.c): every GPU thread owns an independent
// splitmix32-seeded LCG stream, which is why ash_gpu.h already documents
// that the exact risky count differs from a single-threaded run.
//
// Why this is fast on NVIDIA:
//  * Zero memory traffic inside the loop: the whole state of a thread is
//    one 32-bit seed in a register; the only global write is one atomic
//    per block.
//  * No fp64 at all in the loop. The CPU backend tests
//    (s1/2^32) * (s2/2^32) > threshold in doubles; the same test in exact
//    integers is (u64)s1 * s2 > floor(threshold * 2^64), because the left
//    side is an integer and the right side's fractional part cannot change
//    which integers exceed it. That is one IMAD.WIDE plus a 64-bit
//    compare, at full integer rate, instead of two fp64 conversions, a
//    multiply and a compare at the card's 1/64 fp64 rate -- and it is the
//    exact comparison, free of the double product's rounding.
//  * The LCG step is a single IMAD (integer multiply-add, full rate).
//  * Enough resident threads (SMs x 2048) to hide the dependent-IMAD
//    latency of each chain; iterations are divided evenly with the
//    remainder spread one-per-thread, so no thread idles at the end.
//  * Reduction is warp shuffle -> shared -> one 64-bit atomicAdd per block.
#include "H/backends/cuda/context.h"
#include "H/backends/cuda/context_internal.cuh"
#include <math.h>

namespace {

constexpr int BLOCK = 256;
constexpr int BLOCKS_PER_SM = 8;  // 8 x 256 = 2048 threads/SM: full occupancy on Turing..Ada

__device__ __forceinline__ unsigned int splitmix32(unsigned int x) {
    x += 0x9E3779B9u;
    x = (x ^ (x >> 16)) * 0x85EBCA6Bu;
    x = (x ^ (x >> 13)) * 0xC2B2AE35u;
    return x ^ (x >> 16);
}

__global__ void __launch_bounds__(BLOCK)
risk_kernel(long long iterations, unsigned long long cutoff, unsigned int seed,
            unsigned long long *out_risky) {
    const long long total_threads = (long long)gridDim.x * BLOCK;
    const long long tid = (long long)blockIdx.x * BLOCK + threadIdx.x;

    // Even split, remainder spread one extra iteration to the first threads.
    const long long mine = iterations / total_threads + (tid < iterations % total_threads ? 1 : 0);

    unsigned int s = splitmix32(seed + (unsigned int)tid * 2654435761u);
    unsigned int local = 0;  // per-thread count fits 32 bits for any sane split

    for (long long i = 0; i < mine; i++) {
        s = s * 1664525u + 1013904223u;
        const unsigned int s1 = s;
        s = s * 1664525u + 1013904223u;
        local += ((unsigned long long)s1 * s > cutoff) ? 1u : 0u;
    }

    unsigned long long v = local;
#pragma unroll
    for (int d = 16; d > 0; d >>= 1) v += __shfl_down_sync(0xffffffffu, v, d);

    __shared__ unsigned long long warp_sums[BLOCK / 32];
    if ((threadIdx.x & 31) == 0) warp_sums[threadIdx.x >> 5] = v;
    __syncthreads();
    if (threadIdx.x == 0) {
        unsigned long long block_sum = 0;
        for (int w = 0; w < BLOCK / 32; w++) block_sum += warp_sums[w];
        atomicAdd(out_risky, block_sum);
    }
}

}  // namespace

extern "C" int ash_gpu_cuda_monte_carlo_risk(long long iterations, double threshold, unsigned int seed,
                                             long long *out_risky, double *out_probability) {
    if (!ash_gpu_cuda_init() || iterations <= 0) return 0;

    unsigned long long *dev = (unsigned long long *)ash_cuda_scratch(0, sizeof(unsigned long long));
    if (!dev) return 0;
    cudaStream_t st = ash_cuda_stream(0);

    // Map the double threshold onto the integer test in risk_kernel.
    // Products n1*n2 lie in [0, 1): a threshold >= 1 (or NaN) is never
    // exceeded, a threshold < 0 always is (even by a product of 0). In
    // between, scaling by 2^64 is an exact exponent shift and the cast
    // floors it.
    if (!(threshold < 1.0) || threshold < 0.0) {
        *out_risky = threshold < 0.0 ? iterations : 0;
        *out_probability = (double)*out_risky / (double)iterations;
        return 1;
    }
    const unsigned long long cutoff = (unsigned long long)ldexp(threshold, 64);

    unsigned long long host_count = 0;
    bool ok = ash_cuda_ok(cudaMemsetAsync(dev, 0, sizeof(unsigned long long), st));
    if (ok) {
        risk_kernel<<<ash_cuda_sm_count() * BLOCKS_PER_SM, BLOCK, 0, st>>>(iterations, cutoff, seed, dev);
        ok = ash_cuda_ok(cudaGetLastError());
    }
    if (ok) ok = ash_cuda_ok(cudaMemcpyAsync(&host_count, dev, sizeof(host_count), cudaMemcpyDeviceToHost, st));
    if (ok) ok = ash_cuda_ok(cudaStreamSynchronize(st));
    if (!ok) return 0;

    *out_risky = (long long)host_count;
    *out_probability = (double)host_count / (double)iterations;
    return 1;
}
