#include <spdlog/spdlog.h>
#include "vulkan_context.hpp"

#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

namespace vk_sandbox {

    inline VkBool32 debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, VkDebugUtilsMessageTypeFlagsEXT message_type,
                                   const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* p_user_data) {
        if (message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
            spdlog::error("[VulkanContext] {} ", callback_data->pMessage);
        } else if (message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            spdlog::warn("[VulkanContext] {} ", callback_data->pMessage);
        } else if (message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
            spdlog::info("[VulkanContext] {} ", callback_data->pMessage);
        } else if (message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
            spdlog::trace("[VulkanContext] {} ", callback_data->pMessage);
        }
        return VK_FALSE;
    }

    void VulkanContext::init_vulkan() {
        volkInitialize();

        vkb::InstanceBuilder builder(vkGetInstanceProcAddr);
        auto inst_ret = builder.set_app_name("Vulkan Sandbox")
                .require_api_version(1, 1)
                .desire_api_version(1, 2)
                .request_validation_layers()
                .use_default_debug_messenger()
                .set_debug_callback(debug_callback)
                .build();

        instance_builder = inst_ret.value();
        instance = instance_builder.instance;
        debug_messenger = instance_builder.debug_messenger;
        volkLoadInstance(instance);

        vkb::PhysicalDeviceSelector selector{this->instance_builder};

        VkPhysicalDeviceFeatures physical_device_features{};
        physical_device_features.samplerAnisotropy = VK_TRUE;

        vkb::PhysicalDevice _physical_device = selector
                .set_minimum_version(1, 1)
                .set_desired_version(1, 2)
                .set_required_features(physical_device_features)
                .defer_surface_initialization()
                .select()
                .value();

        //create the final Vulkan device
        vkb::DeviceBuilder device_builder{_physical_device};
        vkb::Device vkb_device = device_builder.build().value();

        // Get the VkDevice handle used in the rest of a Vulkan application
        device = vkb_device.device;
        physical_device = _physical_device.physical_device;
        graphics_queue = vkb_device.get_queue(vkb::QueueType::graphics).value();
        graphics_queue_family = vkb_device.get_queue_index(vkb::QueueType::graphics).value();

        for (int i = 0; i < _physical_device.get_queue_families().size(); ++i) {
            if (glfwGetPhysicalDevicePresentationSupport(instance, physical_device, i)) {
                vkGetDeviceQueue(device, i, 0, &present_queue);
                break;
            }
        }

        vkGetPhysicalDeviceProperties(physical_device, &gpu_properties);

        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.physicalDevice = physical_device;
        allocatorInfo.device = device;
        allocatorInfo.instance = instance;
        vmaCreateAllocator(&allocatorInfo, &allocator);

        //create a command pool for commands submitted to the graphics queue.
        VkCommandPoolCreateInfo command_pool_info = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        command_pool_info.queueFamilyIndex = graphics_queue_family;
        command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        vkCreateCommandPool(device, &command_pool_info, nullptr, &command_pool);

        VkCommandBufferAllocateInfo command_buffer_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        command_buffer_info.commandPool = command_pool;
        command_buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        command_buffer_info.commandBufferCount = 1;

        vkAllocateCommandBuffers(device, &command_buffer_info, &temporary_command_buffer);


        std::vector<VkDescriptorPoolSize> sizes = {
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         100},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100},
                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         100}
        };


        VkDescriptorPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.poolSizeCount = sizes.size();
        pool_info.pPoolSizes = sizes.data();
        pool_info.maxSets = 100;

        vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptor_pool);


        spdlog::info("[VulkanContext] Vulkan API {}.{}.{} Device: {} ",
                  VK_VERSION_MAJOR(this->gpu_properties.apiVersion),
                  VK_VERSION_MINOR(this->gpu_properties.apiVersion),
                  VK_VERSION_PATCH(this->gpu_properties.apiVersion),
                  this->gpu_properties.deviceName);
    }

    void VulkanContext::create_swapchain(GLFWwindow* window, bool vsync) {
        glfwCreateWindowSurface(instance, window, nullptr, &surface_khr);

        glm::ivec2 framebuffer_size{};
        glfwGetFramebufferSize(window, &framebuffer_size.x, &framebuffer_size.y);

        vkb::SwapchainBuilder swapchain_builder{physical_device, device, surface_khr};
        vkb::Swapchain vkb_swapchain = swapchain_builder
                .set_desired_format({VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
                .set_desired_present_mode(vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR)
                .set_desired_extent(framebuffer_size.x,framebuffer_size.y)
                .build()
                .value();

        swapchain_khr = vkb_swapchain.swapchain;
        auto vkb_images = vkb_swapchain.get_images().value();
        auto vkb_image_views = vkb_swapchain.get_image_views().value();


    }


    void VulkanContext::destroy_vulkan() {
        vkDeviceWaitIdle(device);
        vkFreeCommandBuffers(device, command_pool, 1, &temporary_command_buffer);
        vkDestroyCommandPool(device, command_pool, nullptr);
        vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
        vmaDestroyAllocator(allocator);
        vkDestroyDevice(device, nullptr);
        vkb::destroy_debug_utils_messenger(instance, debug_messenger);
        vkDestroyInstance(this->instance, nullptr);
    }
}
