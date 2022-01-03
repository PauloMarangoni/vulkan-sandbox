#pragma once

#include "volk.h"
#include "VkBootstrap.h"
#include "vk_mem_alloc.h"

namespace vk_sandbox {

    class VulkanContext {
    public:
        void init_vulkan();
    private:
        vkb::Instance instance_builder{};
        VkInstance instance{};
        VkDebugUtilsMessengerEXT debug_messenger{};
        VkPhysicalDevice physical_device{};
        VkPhysicalDeviceProperties gpu_properties{};
        VkDevice device{};
        VmaAllocator allocator{};
        VkQueue graphics_queue{};
        uint32_t graphics_queue_family{};
        VkQueue present_queue{};
        VkCommandPool command_pool{};
    };
}