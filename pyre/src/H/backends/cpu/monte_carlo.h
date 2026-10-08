#ifndef ASH_GPU_CPU_MONTE_CARLO_H
#define ASH_GPU_CPU_MONTE_CARLO_H
// Internal to the dispatcher (src/C/runtime/device.c) -- not part of
// ash_gpu.h's public ABI. Same contract as ash_gpu_monte_carlo_risk.
void ash_gpu_cpu_monte_carlo_risk(long long iterations, double threshold, unsigned int seed,
                                  long long *out_risky, double *out_probability);
#endif
