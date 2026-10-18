#ifndef ASH_GPU_VULKAN_CONTEXT_INTERNAL_HPP
#define ASH_GPU_VULKAN_CONTEXT_INTERNAL_HPP
// C++-only (no extern "C" -- this is never called from matrix.c/device.c
// directly, only from other .cpp files in this same backend) companion
// to context.h: the actual Vulkan handles context.cpp's
// ash_gpu_vulkan_init() creates, that matrix_multiply.cpp needs to issue
// its own dispatch against the same instance/device/queue instead of
// creating a second one.
#include <vulkan/vulkan.h>

struct AshGpuVulkanContext {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue compute_queue = VK_NULL_HANDLE;
    uint32_t compute_queue_family = 0;
    VkCommandPool command_pool = VK_NULL_HANDLE;
};

// Valid only after ash_gpu_vulkan_init() (context.h) has returned 1.
const AshGpuVulkanContext &ash_gpu_vulkan_context(void);

#endif
