// CUDA kernel for ash_gpu_chaos_iterate (ash_gpu.h): data[i] =
// iterate(data[i]), x = sqrt(|x|+1)*0.99999 + 0.00001, `iterations` times.
//
// Why this is fast on NVIDIA:
//  * The recurrence is a serial dependency chain per element, dominated by
//    the fp64 sqrt (long latency, low throughput on consumer parts). Each
//    thread therefore advances ILP independent elements in lock step so the
//    chains overlap instead of the warp stalling on one.
//  * Elements of a block are contiguous and a thread's ILP elements are one
//    block-width apart, so every load/store is a fully coalesced 256 B warp
//    access.
//  * Host<->device traffic is split into chunks over several streams, so
//    chunk k's kernel overlaps chunk k+1's upload and chunk k-1's download.
//
// __dmul_rn / __dadd_rn instead of `*` and `+`: nvcc would otherwise fuse
// them into one FMA, which rounds once instead of twice and would make the
// result differ from the CPU backend (compiled -std=c11, no contraction) in
// the last bit. Paying one extra DADD per step keeps CPU and GPU results
// bit-identical, which the benchmark checks.
#include "H/backends/cuda/context.h"
#include "H/backends/cuda/context_internal.cuh"
#include <algorithm>

namespace {

constexpr int BLOCK = 256;
constexpr int ILP = 4;
// Chunks smaller than this are not worth a separate stream round trip.
constexpr int MIN_CHUNK = 1 << 18;

__global__ void __launch_bounds__(BLOCK) chaos_kernel(double *data, int n, int iterations) {
    const int base = blockIdx.x * (BLOCK * ILP) + threadIdx.x;

    double x[ILP];
    bool live[ILP];
#pragma unroll
    for (int j = 0; j < ILP; j++) {
        const int i = base + j * BLOCK;
        live[j] = i < n;
        x[j] = live[j] ? data[i] : 0.0;
    }

    for (int k = 0; k < iterations; k++) {
#pragma unroll
        for (int j = 0; j < ILP; j++) {
            x[j] = __dadd_rn(__dmul_rn(sqrt(__dadd_rn(fabs(x[j]), 1.0)), 0.99999), 0.00001);
        }
    }

#pragma unroll
    for (int j = 0; j < ILP; j++) {
        const int i = base + j * BLOCK;
        if (live[j]) data[i] = x[j];
    }
}

}  // namespace

extern "C" int ash_gpu_cuda_chaos_iterate(double *data, int n, int iterations) {
    if (!ash_gpu_cuda_init() || n <= 0) return 0;

    const size_t bytes = (size_t)n * sizeof(double);
    double *dev = (double *)ash_cuda_scratch(0, bytes);
    if (!dev) return 0;

    const int pinned = ash_cuda_pin(data, bytes);

    int streams = std::min(ASH_CUDA_STREAMS, std::max(1, n / MIN_CHUNK));
    const int chunk = (n + streams - 1) / streams;

    bool ok = true;
    for (int s = 0; s < streams && ok; s++) {
        const int off = s * chunk;
        const int len = std::min(chunk, n - off);
        if (len <= 0) break;
        cudaStream_t st = ash_cuda_stream(s);
        ok = ash_cuda_ok(cudaMemcpyAsync(dev + off, data + off, (size_t)len * sizeof(double),
                                         cudaMemcpyHostToDevice, st));
        if (!ok) break;
        const int blocks = (len + BLOCK * ILP - 1) / (BLOCK * ILP);
        chaos_kernel<<<blocks, BLOCK, 0, st>>>(dev + off, len, iterations);
        ok = ash_cuda_ok(cudaGetLastError());
        if (!ok) break;
        ok = ash_cuda_ok(cudaMemcpyAsync(data + off, dev + off, (size_t)len * sizeof(double),
                                         cudaMemcpyDeviceToHost, st));
    }

    for (int s = 0; s < streams; s++) {
        if (!ash_cuda_ok(cudaStreamSynchronize(ash_cuda_stream(s)))) ok = false;
    }
    ash_cuda_unpin(data, pinned);
    return ok ? 1 : 0;
}
