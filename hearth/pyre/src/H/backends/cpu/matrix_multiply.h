#ifndef ASH_GPU_CPU_MATRIX_MULTIPLY_H
#define ASH_GPU_CPU_MATRIX_MULTIPLY_H
// Internal to the dispatcher (src/C/runtime/device.c) -- not part of
// ash_gpu.h's public ABI. Same contract as ash_gpu_matrix_multiply.
void ash_gpu_cpu_matrix_multiply(const double *a, const double *b, double *c, int n);
#endif
