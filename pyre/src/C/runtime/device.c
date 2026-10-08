#include "H/runtime/ash_gpu.h"
#include "H/backends/vulkan/context.h"
#include "H/backends/cpu/matrix_multiply.h"
#include "H/backends/cpu/chaos.h"
#include <string.h>

/*
 * Real detection: tries to actually bring up a Vulkan device (not just
 * check a loader is present -- see ash_gpu_vulkan_init(), src/C/backends/vulkan/context.cpp).
 * This turned out to be WSL2 with GPU passthrough available but no
 * Vulkan driver installed at first -- installing vulkan-dzn (Mesa's
 * Vulkan-over-D3D12 driver) made this report ASH_GPU_BACKEND_VULKAN for
 * real, naming the user's actual AMD Radeon RX 9070 XT. See
 * src/C/backends/vulkan/context.cpp's header comment.
 */
AshGpuInfo ash_gpu_detect(void) {
    AshGpuInfo info;
    info.vram_bytes = 0;

    if (ash_gpu_vulkan_init()) {
        info.backend = ASH_GPU_BACKEND_VULKAN;
        strncpy(info.device_name, ash_gpu_vulkan_device_name(), sizeof(info.device_name) - 1);
    } else {
        info.backend = ASH_GPU_BACKEND_NONE;
        strncpy(info.device_name, "CPU fallback", sizeof(info.device_name) - 1);
    }
    info.device_name[sizeof(info.device_name) - 1] = '\0';
    return info;
}

int ash_gpu_available(void) {
    return ash_gpu_detect().backend != ASH_GPU_BACKEND_NONE;
}

// Both dispatchers unconditionally prefer GPU-when-available over CPU,
// without comparing measured cost first -- matrix_multiply's own real
// measurement says that's the WRONG call for that kernel on this
// hardware (ash_gpu.h's comment on it); chaos_iterate exists precisely
// because it's shaped to actually win on GPU instead. Neither dispatcher
// has been taught to tell the difference yet -- see ideas/assigned.md.
void ash_gpu_matrix_multiply(const double *a, const double *b, double *c, int n) {
    if (ash_gpu_available()) {
        ash_gpu_vulkan_matrix_multiply(a, b, c, n);
        return;
    }
    ash_gpu_cpu_matrix_multiply(a, b, c, n);
}

void ash_gpu_chaos_iterate(double *data, int n, int iterations) {
    if (ash_gpu_available()) {
        ash_gpu_vulkan_chaos_iterate(data, n, iterations);
        return;
    }
    ash_gpu_cpu_chaos_iterate(data, n, iterations);
}
