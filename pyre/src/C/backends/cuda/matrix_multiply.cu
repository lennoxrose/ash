// CUDA kernel for ash_gpu_matrix_multiply (ash_gpu.h): C = A * B, all N x N,
// row-major, fp64 (the ABI is `double` throughout).
//
// Honest hardware note: GeForce parts run fp64 at 1/64 of their fp32 rate
// (a laptop RTX 3050 peaks near 100 GFLOP/s of fp64), and fp64 has no tensor
// core path on consumer Ampere/Ada. This kernel's job is to keep that small
// fp64 pipe saturated, not to beat a many-core CPU on a card that
// physically cannot; the benchmark reports both numbers.
//
// Structure (the classic high-end SGEMM shape, adapted to fp64):
//  * 64x64 output tile per 16x16-thread block, 4x4 outputs per thread held
//    in registers: each loaded value feeds 4 FMAs, so shared-memory
//    traffic per FMA is a quarter of the naive kernel's.
//  * BK=16 deep k-slices staged through shared memory. A is stored
//    transposed with a 65-double row stride so the transposing store is
//    bank-conflict free; B is read as double2 (16 B) vectors.
//  * Software-pipelined: the next k-slice is fetched into registers while
//    the current one is multiplied, then written to the other shared
//    buffer -- one __syncthreads per slice, global-load latency hidden.
//  * Tile loads are bounds-checked and zero-filled, so any N works, not
//    just multiples of 64.
//  * Host side splits C by rows over streams: B uploads once, then each
//    stream uploads its rows of A, multiplies, and downloads its rows of C
//    while the other streams compute.
#include "H/backends/cuda/context.h"
#include "H/backends/cuda/context_internal.cuh"
#include <algorithm>

namespace {

constexpr int BM = 64, BN = 64, BK = 16;
constexpr int TM = 4, TN = 4;
constexpr int THREADS_X = BN / TN;  // 16
constexpr int THREADS_Y = BM / TM;  // 16
constexpr int THREADS = THREADS_X * THREADS_Y;
constexpr int A_STRIDE = BM + 1;    // odd stride: conflict-free transposed store
constexpr int A_LOADS = (BM * BK) / THREADS;  // 4
constexpr int B_LOADS = (BK * BN) / THREADS;  // 4

// C[m x n] = A[m x n] * B[n x n]  (A and C are a horizontal band of rows).
__global__ void __launch_bounds__(THREADS)
gemm_kernel(const double *__restrict__ A, const double *__restrict__ B,
            double *__restrict__ C, int m, int n) {
    __shared__ double As[2][BK][A_STRIDE];
    __shared__ __align__(16) double Bs[2][BK][BN];

    const int tx = threadIdx.x, ty = threadIdx.y;
    const int tid = ty * THREADS_X + tx;
    const int row0 = blockIdx.y * BM;
    const int col0 = blockIdx.x * BN;

    double acc[TM][TN];
#pragma unroll
    for (int i = 0; i < TM; i++)
#pragma unroll
        for (int j = 0; j < TN; j++) acc[i][j] = 0.0;

    double ra[A_LOADS], rb[B_LOADS];

    // Fetch k-slice `k0` of A and B into registers (zero outside the matrix).
    auto fetch = [&](int k0) {
#pragma unroll
        for (int i = 0; i < A_LOADS; i++) {
            const int idx = tid + i * THREADS;
            const int r = row0 + (idx / BK), k = k0 + (idx % BK);
            ra[i] = (r < m && k < n) ? A[(size_t)r * n + k] : 0.0;
        }
#pragma unroll
        for (int i = 0; i < B_LOADS; i++) {
            const int idx = tid + i * THREADS;
            const int k = k0 + (idx / BN), c = col0 + (idx % BN);
            rb[i] = (k < n && c < n) ? B[(size_t)k * n + c] : 0.0;
        }
    };
    auto commit = [&](int buf) {
#pragma unroll
        for (int i = 0; i < A_LOADS; i++) {
            const int idx = tid + i * THREADS;
            As[buf][idx % BK][idx / BK] = ra[i];
        }
#pragma unroll
        for (int i = 0; i < B_LOADS; i++) {
            const int idx = tid + i * THREADS;
            Bs[buf][idx / BN][idx % BN] = rb[i];
        }
    };

    const int tiles = (n + BK - 1) / BK;
    fetch(0);
    commit(0);
    __syncthreads();

    for (int t = 0; t < tiles; t++) {
        const int cur = t & 1;
        const bool more = t + 1 < tiles;
        if (more) fetch((t + 1) * BK);

#pragma unroll
        for (int kk = 0; kk < BK; kk++) {
            double a[TM], b[TN];
#pragma unroll
            for (int i = 0; i < TM; i++) a[i] = As[cur][kk][ty * TM + i];
            const double2 b01 = *reinterpret_cast<const double2 *>(&Bs[cur][kk][tx * TN]);
            const double2 b23 = *reinterpret_cast<const double2 *>(&Bs[cur][kk][tx * TN + 2]);
            b[0] = b01.x; b[1] = b01.y; b[2] = b23.x; b[3] = b23.y;
#pragma unroll
            for (int i = 0; i < TM; i++)
#pragma unroll
                for (int j = 0; j < TN; j++) acc[i][j] = fma(a[i], b[j], acc[i][j]);
        }

        if (more) commit(cur ^ 1);
        __syncthreads();
    }

#pragma unroll
    for (int i = 0; i < TM; i++) {
        const int r = row0 + ty * TM + i;
        if (r >= m) continue;
#pragma unroll
        for (int j = 0; j < TN; j++) {
            const int c = col0 + tx * TN + j;
            if (c < n) C[(size_t)r * n + c] = acc[i][j];
        }
    }
}

}  // namespace

extern "C" int ash_gpu_cuda_matrix_multiply(const double *a, const double *b, double *c, int n) {
    if (!ash_gpu_cuda_init() || n <= 0) return 0;

    const size_t mat_bytes = (size_t)n * n * sizeof(double);
    double *dA = (double *)ash_cuda_scratch(0, mat_bytes);
    double *dB = (double *)ash_cuda_scratch(1, mat_bytes);
    double *dC = (double *)ash_cuda_scratch(2, mat_bytes);
    if (!dA || !dB || !dC) return 0;

    const int pin_a = ash_cuda_pin(a, mat_bytes);
    const int pin_b = ash_cuda_pin(b, mat_bytes);
    const int pin_c = ash_cuda_pin(c, mat_bytes);

    // Row bands are multiples of the tile height so no tile straddles two
    // streams. Tiny matrices stay on one stream.
    const int bands_wanted = n >= 512 ? ASH_CUDA_STREAMS : 1;
    const int band = ((n + bands_wanted - 1) / bands_wanted + BM - 1) / BM * BM;

    bool ok = true;

    // B is needed by every band: upload it once on stream 0 and make the
    // other streams wait for that event.
    cudaStream_t s0 = ash_cuda_stream(0);
    ok = ash_cuda_ok(cudaMemcpyAsync(dB, b, mat_bytes, cudaMemcpyHostToDevice, s0));
    cudaEvent_t b_ready = NULL;
    if (ok) ok = ash_cuda_ok(cudaEventCreateWithFlags(&b_ready, cudaEventDisableTiming));
    if (ok) ok = ash_cuda_ok(cudaEventRecord(b_ready, s0));

    int used = 0;
    for (int s = 0; s < bands_wanted && ok; s++) {
        const int r0 = s * band;
        const int rows = std::min(band, n - r0);
        if (rows <= 0) break;
        used = s + 1;
        cudaStream_t st = ash_cuda_stream(s);
        const size_t off = (size_t)r0 * n;
        const size_t band_bytes = (size_t)rows * n * sizeof(double);

        ok = ash_cuda_ok(cudaMemcpyAsync(dA + off, a + off, band_bytes, cudaMemcpyHostToDevice, st));
        if (!ok) break;
        ok = ash_cuda_ok(cudaStreamWaitEvent(st, b_ready, 0));
        if (!ok) break;

        const dim3 block(THREADS_X, THREADS_Y);
        const dim3 grid((n + BN - 1) / BN, (rows + BM - 1) / BM);
        gemm_kernel<<<grid, block, 0, st>>>(dA + off, dB, dC + off, rows, n);
        ok = ash_cuda_ok(cudaGetLastError());
        if (!ok) break;

        ok = ash_cuda_ok(cudaMemcpyAsync(c + off, dC + off, band_bytes, cudaMemcpyDeviceToHost, st));
    }

    for (int s = 0; s < std::max(used, 1); s++) {
        if (!ash_cuda_ok(cudaStreamSynchronize(ash_cuda_stream(s)))) ok = false;
    }
    if (b_ready) cudaEventDestroy(b_ready);
    ash_cuda_unpin(a, pin_a);
    ash_cuda_unpin(b, pin_b);
    ash_cuda_unpin(c, pin_c);
    return ok ? 1 : 0;
}
