#include <spdlog/spdlog.h>

#define VK_NO_PROTOTYPES

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

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


        spdlog::info("[VulkanContext] Vulkan API {}.{}.{} Device: {} ",
                  VK_VERSION_MAJOR(this->gpu_properties.apiVersion),
                  VK_VERSION_MINOR(this->gpu_properties.apiVersion),
                  VK_VERSION_PATCH(this->gpu_properties.apiVersion),
                  this->gpu_properties.deviceName);
    }
}
