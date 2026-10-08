// Vulkan instance/device setup -- see H/backends/vulkan/context.h for
// the contract. Written and compiled against the real Vulkan 1.2 SDK
// (pyre/tools/install_deps.sh), but never run end-to-end: this sandbox
// has no GPU and no ICD at all (vulkaninfo reports "Found no drivers!"),
// so vkCreateInstance itself has never actually succeeded here. Treat
// this file as reviewed-and-compiles, not verified-at-runtime, until
// someone runs it on real hardware (the user's RX 9070 XT).
#include "H/backends/vulkan/context.h"
#include "H/backends/vulkan/context_internal.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <cstring>

namespace {

enum class InitState { NotTried, Ready, Unavailable };
InitState g_state = InitState::NotTried;
AshGpuVulkanContext g_ctx;
char g_device_name[256] = "";

void destroy_partial(AshGpuVulkanContext &ctx) {
    if (ctx.command_pool != VK_NULL_HANDLE) vkDestroyCommandPool(ctx.device, ctx.command_pool, nullptr);
    if (ctx.device != VK_NULL_HANDLE) vkDestroyDevice(ctx.device, nullptr);
    if (ctx.instance != VK_NULL_HANDLE) vkDestroyInstance(ctx.instance, nullptr);
    ctx = AshGpuVulkanContext{};
}

// First physical device with both a compute-capable queue family and
// the shaderFloat64 feature (ash_gpu's ABI is `double` throughout, see
// ash_gpu.h) -- a device that can only do fp32 compute isn't a match.
bool pick_physical_device(VkInstance instance, VkPhysicalDevice *out_device, uint32_t *out_queue_family) {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    if (count == 0) return false;
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());

    for (VkPhysicalDevice dev : devices) {
        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(dev, &features);
        if (!features.shaderFloat64) continue;

        uint32_t qf_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qf_count, nullptr);
        std::vector<VkQueueFamilyProperties> qfs(qf_count);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qf_count, qfs.data());

        for (uint32_t i = 0; i < qf_count; i++) {
            if (qfs[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                *out_device = dev;
                *out_queue_family = i;
                return true;
            }
        }
    }
    return false;
}

bool init_once(AshGpuVulkanContext &ctx) {
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "pyre";
    app_info.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo instance_info{};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;
    // Headless compute only -- no VK_KHR_surface/swapchain, so no
    // extensions or layers are needed at all for this kernel.

    if (vkCreateInstance(&instance_info, nullptr, &ctx.instance) != VK_SUCCESS) return false;

    if (!pick_physical_device(ctx.instance, &ctx.physical_device, &ctx.compute_queue_family)) return false;

    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info{};
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = ctx.compute_queue_family;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &queue_priority;

    VkPhysicalDeviceFeatures enabled_features{};
    enabled_features.shaderFloat64 = VK_TRUE;

    VkDeviceCreateInfo device_info{};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;
    device_info.pEnabledFeatures = &enabled_features;

    if (vkCreateDevice(ctx.physical_device, &device_info, nullptr, &ctx.device) != VK_SUCCESS) return false;

    vkGetDeviceQueue(ctx.device, ctx.compute_queue_family, 0, &ctx.compute_queue);

    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = ctx.compute_queue_family;
    if (vkCreateCommandPool(ctx.device, &pool_info, nullptr, &ctx.command_pool) != VK_SUCCESS) return false;

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(ctx.physical_device, &props);
    std::strncpy(g_device_name, props.deviceName, sizeof(g_device_name) - 1);
    g_device_name[sizeof(g_device_name) - 1] = '\0';

    return true;
}

} // namespace

extern "C" int ash_gpu_vulkan_init(void) {
    if (g_state == InitState::NotTried) {
        AshGpuVulkanContext ctx;
        if (init_once(ctx)) {
            g_ctx = ctx;
            g_state = InitState::Ready;
        } else {
            destroy_partial(ctx);
            g_state = InitState::Unavailable;
        }
    }
    return g_state == InitState::Ready ? 1 : 0;
}

extern "C" const char *ash_gpu_vulkan_device_name(void) { return g_device_name; }

const AshGpuVulkanContext &ash_gpu_vulkan_context(void) { return g_ctx; }
