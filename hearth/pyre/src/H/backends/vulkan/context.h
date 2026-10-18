#ifndef ASH_GPU_VULKAN_CONTEXT_H
#define ASH_GPU_VULKAN_CONTEXT_H
// Internal to the Vulkan backend (not part of ash_gpu.h's public ABI):
// a process-lifetime instance/device singleton, lazily created on first
// use, plus the one kernel implemented so far. C linkage throughout --
// runtime/device.c (plain C) calls ash_gpu_vulkan_init() to decide what
// ash_gpu_detect() reports, even though this backend's own .cpp files
// use Vulkan's C++-friendly RAII patterns internally.
#ifdef __cplusplus
extern "C" {
#endif

// Creates a VkInstance and picks the first physical device that has a
// compute-capable queue family AND the shaderFloat64 feature (ash_gpu's
// C ABI is `double` throughout -- see H/runtime/ash_gpu.h -- so a device
// that can only do fp32 compute isn't a match for this kernel). Returns
// 1 if a usable device is now ready to dispatch work, 0 for any other
// outcome (no Vulkan loader, no device, no fp64 support) -- all of
// those are normal "no GPU here" answers, never a reason to abort.
// Idempotent: after the first call, later calls just return the cached
// result instead of re-probing.
int ash_gpu_vulkan_init(void);

// Valid only after ash_gpu_vulkan_init() returned 1. The physical
// device's name (VkPhysicalDeviceProperties.deviceName), e.g. "AMD
// Radeon RX 9070 XT" -- for ash_gpu_detect()'s AshGpuInfo.device_name.
const char *ash_gpu_vulkan_device_name(void);

// Only valid once ash_gpu_vulkan_init() has returned 1. Same contract as
// ash_gpu_matrix_multiply (ash_gpu.h): C = A * B, all three N x N,
// row-major, contiguous, C must not alias A or B.
void ash_gpu_vulkan_matrix_multiply(const double *a, const double *b, double *c, int n);

// Same contract as ash_gpu_chaos_iterate (ash_gpu.h): data[i] =
// iterate(data[i]) in place, `iterations` times each.
void ash_gpu_vulkan_chaos_iterate(double *data, int n, int iterations);

#ifdef __cplusplus
}
#endif
#endif
