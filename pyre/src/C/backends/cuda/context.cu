// Pyre's CUDA backend (NVIDIA only): process-lifetime device, streams and
// scratch-buffer state, shared by the kernel files next to this one.
// See src/H/backends/cuda/context.h for the contract.
#include "H/backends/cuda/context.h"
#include "H/backends/cuda/context_internal.cuh"
#include <stdio.h>

namespace {

struct CudaState {
    int tried = 0;
    int ready = 0;
    int sm_count = 0;
    char name[256] = {0};
    unsigned long long vram = 0;
    cudaStream_t streams[ASH_CUDA_STREAMS] = {};
    void *scratch[ASH_CUDA_SCRATCH_SLOTS] = {};
    size_t scratch_bytes[ASH_CUDA_SCRATCH_SLOTS] = {};
};

CudaState g;

// Registration (page-locking) costs roughly a millisecond per few MB; below
// this size the staging copy it would save is cheaper.
const size_t PIN_MIN_BYTES = 1u << 20;

int init_once() {
    int count = 0;
    if (cudaGetDeviceCount(&count) != cudaSuccess || count == 0) {
        (void)cudaGetLastError();
        return 0;
    }

    // Most multiprocessors wins: on a multi-GPU box that is the one the
    // user bought for compute, not the one driving the display.
    int best = -1, best_sms = -1;
    for (int d = 0; d < count; d++) {
        cudaDeviceProp p;
        if (cudaGetDeviceProperties(&p, d) != cudaSuccess) continue;
        if (p.multiProcessorCount > best_sms) { best_sms = p.multiProcessorCount; best = d; }
    }
    if (best < 0 || cudaSetDevice(best) != cudaSuccess) return 0;

    cudaDeviceProp props;
    if (cudaGetDeviceProperties(&props, best) != cudaSuccess) return 0;
    snprintf(g.name, sizeof(g.name), "%s", props.name);
    g.vram = (unsigned long long)props.totalGlobalMem;
    g.sm_count = props.multiProcessorCount;

    for (int i = 0; i < ASH_CUDA_STREAMS; i++) {
        if (cudaStreamCreateWithFlags(&g.streams[i], cudaStreamNonBlocking) != cudaSuccess) return 0;
    }
    // Touch the context now so a broken driver fails detection, not the
    // first real dispatch.
    if (cudaFree(0) != cudaSuccess) return 0;
    return 1;
}

}  // namespace

extern "C" int ash_gpu_cuda_init(void) {
    if (!g.tried) {
        g.tried = 1;
        g.ready = init_once();
        if (!g.ready) (void)cudaGetLastError();
    }
    return g.ready;
}

extern "C" const char *ash_gpu_cuda_device_name(void) { return g.name; }
extern "C" unsigned long long ash_gpu_cuda_vram_bytes(void) { return g.vram; }

cudaStream_t ash_cuda_stream(int index) { return g.streams[index]; }
int ash_cuda_sm_count(void) { return g.sm_count; }

void *ash_cuda_scratch(int slot, size_t bytes) {
    if (g.scratch_bytes[slot] >= bytes) return g.scratch[slot];
    if (g.scratch[slot]) {
        cudaFree(g.scratch[slot]);
        g.scratch[slot] = NULL;
        g.scratch_bytes[slot] = 0;
    }
    void *p = NULL;
    if (cudaMalloc(&p, bytes) != cudaSuccess) {
        (void)cudaGetLastError();
        return NULL;
    }
    g.scratch[slot] = p;
    g.scratch_bytes[slot] = bytes;
    return p;
}

int ash_cuda_pin(const void *ptr, size_t bytes) {
    if (bytes < PIN_MIN_BYTES) return 0;
    if (cudaHostRegister(const_cast<void *>(ptr), bytes, cudaHostRegisterDefault) != cudaSuccess) {
        (void)cudaGetLastError();
        return 0;
    }
    return 1;
}

void ash_cuda_unpin(const void *ptr, int pinned) {
    if (pinned) cudaHostUnregister(const_cast<void *>(ptr));
}
