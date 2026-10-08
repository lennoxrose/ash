#ifndef ASH_GPU_CUDA_CONTEXT_INTERNAL_CUH
#define ASH_GPU_CUDA_CONTEXT_INTERNAL_CUH
// Shared by the CUDA backend's .cu files only (context.cu owns the state).
#include <cuda_runtime.h>
#include <stddef.h>

// True when a CUDA call succeeded; on failure also clears the sticky
// error so the next call starts clean.
static inline bool ash_cuda_ok(cudaError_t e) {
    if (e == cudaSuccess) return true;
    (void)cudaGetLastError();
    return false;
}

// Streams the pipelined kernels spread chunks over. Index < ASH_CUDA_STREAMS.
#define ASH_CUDA_STREAMS 4
cudaStream_t ash_cuda_stream(int index);

// Grow-only device scratch buffers, reused across calls so repeated
// dispatches (ashvm calling matrix_mul in a loop) pay cudaMalloc once.
// Slot 0..2. Returns NULL if the allocation fails.
#define ASH_CUDA_SCRATCH_SLOTS 3
void *ash_cuda_scratch(int slot, size_t bytes);

// Page-locks a caller's malloc'd buffer so cudaMemcpyAsync can DMA straight
// from/to it (no staging copy) and overlap with kernels. Skipped for small
// buffers where registration costs more than it saves. Failure is harmless:
// async copies from pageable memory still work, just without the overlap.
int ash_cuda_pin(const void *ptr, size_t bytes);
void ash_cuda_unpin(const void *ptr, int pinned);

int ash_cuda_sm_count(void);

#endif
