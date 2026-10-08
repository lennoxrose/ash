#ifndef ASH_GPU_CPU_CHAOS_H
#define ASH_GPU_CPU_CHAOS_H
// Internal to the dispatcher (src/C/runtime/device.c) -- not part of
// ash_gpu.h's public ABI. Same contract as ash_gpu_chaos_iterate.
void ash_gpu_cpu_chaos_iterate(double *data, int n, int iterations);
#endif
