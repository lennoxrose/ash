// Vulkan compute dispatch for chaos_iterate -- see
// src/H/runtime/ash_gpu.h's comment on why this kernel exists
// (matrix_multiply's real GPU measurement came back a loss; this one is
// shaped to actually favor the GPU) and src/shaders/chaos.comp for the
// recurrence itself. One buffer, in place -- simpler than
// matrix_multiply.cpp's three (no separate input/output, no second
// binding).
#include "H/backends/vulkan/context.h"
#include "H/backends/vulkan/context_internal.hpp"
#include <vulkan/vulkan.h>
#include <cstring>
#include <cstdint>

#include "H/backends/vulkan/chaos_spv.h"

namespace {

uint32_t find_memory_type(VkPhysicalDevice phys, uint32_t type_filter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(phys, &mem_props);
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((type_filter & (1u << i)) && (mem_props.memoryTypes[i].propertyFlags & props) == props) return i;
    }
    return UINT32_MAX;
}

} // namespace

extern "C" void ash_gpu_vulkan_chaos_iterate(double *data, int n, int iterations) {
    const AshGpuVulkanContext &ctx = ash_gpu_vulkan_context();
    VkDevice device = ctx.device;
    VkDeviceSize bytes = (VkDeviceSize)n * sizeof(double);

    VkBufferCreateInfo buf_info{};
    buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_info.size = bytes;
    buf_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer buffer = VK_NULL_HANDLE;
    if (vkCreateBuffer(device, &buf_info, nullptr, &buffer) != VK_SUCCESS) return;

    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(device, buffer, &mem_req);
    uint32_t mem_type = find_memory_type(ctx.physical_device, mem_req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (mem_type == UINT32_MAX) { vkDestroyBuffer(device, buffer, nullptr); return; }

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_req.size;
    alloc_info.memoryTypeIndex = mem_type;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (vkAllocateMemory(device, &alloc_info, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyBuffer(device, buffer, nullptr);
        return;
    }
    vkBindBufferMemory(device, buffer, memory, 0);

    void *mapped = nullptr;
    vkMapMemory(device, memory, 0, bytes, 0, &mapped);
    memcpy(mapped, data, (size_t)bytes);

    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &binding;
    VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
    vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &set_layout);

    struct PushConstants { uint32_t n; uint32_t iterations; } push{(uint32_t)n, (uint32_t)iterations};
    VkPushConstantRange push_range{};
    push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    push_range.offset = 0;
    push_range.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &set_layout;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push_range;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline_layout);

    VkShaderModuleCreateInfo shader_info{};
    shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_info.codeSize = sizeof(chaos_spv);
    shader_info.pCode = chaos_spv;
    VkShaderModule shader_module = VK_NULL_HANDLE;
    vkCreateShaderModule(device, &shader_info, nullptr, &shader_module);

    VkComputePipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipeline_info.stage.module = shader_module;
    pipeline_info.stage.pName = "main";
    pipeline_info.layout = pipeline_layout;
    VkPipeline pipeline = VK_NULL_HANDLE;
    vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline);

    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pool_size.descriptorCount = 1;
    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.maxSets = 1;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;
    VkDescriptorPool desc_pool = VK_NULL_HANDLE;
    vkCreateDescriptorPool(device, &pool_info, nullptr, &desc_pool);

    VkDescriptorSetAllocateInfo set_alloc_info{};
    set_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set_alloc_info.descriptorPool = desc_pool;
    set_alloc_info.descriptorSetCount = 1;
    set_alloc_info.pSetLayouts = &set_layout;
    VkDescriptorSet desc_set = VK_NULL_HANDLE;
    vkAllocateDescriptorSets(device, &set_alloc_info, &desc_set);

    VkDescriptorBufferInfo buf_info_desc{buffer, 0, bytes};
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = desc_set;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &buf_info_desc;
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

    VkCommandBufferAllocateInfo cmd_alloc_info{};
    cmd_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_alloc_info.commandPool = ctx.command_pool;
    cmd_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_alloc_info.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(device, &cmd_alloc_info, &cmd);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin_info);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1, &desc_set, 0, nullptr);
    vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);
    uint32_t groups = ((uint32_t)n + 255u) / 256u; // matches local_size_x = 256 in the shader
    vkCmdDispatch(cmd, groups, 1, 1);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;
    vkQueueSubmit(ctx.compute_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx.compute_queue);

    memcpy(data, mapped, (size_t)bytes);

    vkFreeCommandBuffers(device, ctx.command_pool, 1, &cmd);
    vkDestroyDescriptorPool(device, desc_pool, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyShaderModule(device, shader_module, nullptr);
    vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(device, set_layout, nullptr);
    vkUnmapMemory(device, memory);
    vkDestroyBuffer(device, buffer, nullptr);
    vkFreeMemory(device, memory, nullptr);
}
