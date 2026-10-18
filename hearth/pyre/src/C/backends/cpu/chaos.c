#include "H/backends/cpu/chaos.h"
#include <math.h>

// CPU backend for chaos_iterate -- always available. See ash_gpu.h for
// why this kernel exists (matrix_multiply's real GPU measurement came
// back a loss; this one is shaped to actually favor the GPU).

// Same recurrence as src/shaders/chaos.comp, exactly -- see that file's
// comment for why it's sqrt/abs-based rather than sin/cos (GLSL's
// fp64 extension doesn't give sin/cos a double overload at all, even
// though it covers sqrt/abs; keeping both backends on the identical
// formula makes the CPU-vs-GPU comparison apples-to-apples).
void ash_gpu_cpu_chaos_iterate(double *data, int n, int iterations) {
#pragma omp parallel for schedule(static)
    for (int i = 0; i < n; i++) {
        double x = data[i];
        for (int k = 0; k < iterations; k++) {
            x = sqrt(fabs(x) + 1.0) * 0.99999 + 0.00001;
        }
        data[i] = x;
    }
}
