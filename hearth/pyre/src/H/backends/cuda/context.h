#ifndef ASH_GPU_CUDA_CONTEXT_H
#define ASH_GPU_CUDA_CONTEXT_H
// Internal to the dispatcher (src/C/runtime/device.c) -- not part of
// ash_gpu.h's public ABI. NVIDIA-only backend, CUDA runtime API, kernels
// in src/C/backends/cuda/*.cu. Only compiled when pyre/cuda.mk finds
// nvcc (ASH_GPU_HAVE_CUDA); device.c never references this header
// otherwise.
//
// Every kernel entry point returns 1 when the GPU did the work and 0 when
// it could not (out of device memory, launch failure, ...): the
// dispatcher then runs the CPU backend, so "no GPU" and "GPU failed" are
// both a normal answer, never an error (design principle 1,
// ideas/gpu_acceleration.md). Not thread-safe: one caller at a time, like
// the Vulkan backend's singleton.
#ifdef __cplusplus
extern "C" {
#endif

// Brings up the best CUDA device (most multiprocessors) once; later calls
// return the cached result. 1 = a CUDA device is ready, 0 = none (no
// driver, no NVIDIA GPU, runtime error).
int ash_gpu_cuda_init(void);

// Valid after ash_gpu_cuda_init() returned 1.
const char *ash_gpu_cuda_device_name(void);
unsigned long long ash_gpu_cuda_vram_bytes(void);

// Same contracts as ash_gpu_matrix_multiply / ash_gpu_chaos_iterate /
// ash_gpu_monte_carlo_risk in ash_gpu.h.
int ash_gpu_cuda_matrix_multiply(const double *a, const double *b, double *c, int n);
int ash_gpu_cuda_chaos_iterate(double *data, int n, int iterations);
int ash_gpu_cuda_monte_carlo_risk(long long iterations, double threshold, unsigned int seed,
                                  long long *out_risky, double *out_probability);

#ifdef __cplusplus
}
#endif
#endif
