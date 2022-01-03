#pragma once

#include "volk.h"
#include "VkBootstrap.h"
#include "vk_mem_alloc.h"

#define VK_NO_PROTOTYPES
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include <glm/glm.hpp>
#include "vulkan_types.hpp"

namespace vk_sandbox {

    class VulkanContext {
    public:
        void init_vulkan();
        void destroy_vulkan();
        void create_swapchain(GLFWwindow* window, bool vsync);
        VulkanRenderTarget create_render_target();
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
        VkCommandBuffer temporary_command_buffer{};
        VkDescriptorPool descriptor_pool{};

        //swap chain
        VkSurfaceKHR surface_khr{};
        VkSwapchainKHR swapchain_khr{};
        std::vector<VkCommandBuffer> cmds;
        std::vector<VulkanImage> images;
        VkRenderPass swapchain_renderpass;
        std::vector<VkFramebuffer> swapchain_framebuffers;

        //sync
        size_t current_frame = 0;
        std::vector<VkFence> images_in_flight{};
        std::vector<VkSemaphore> image_available_semaphores;
        std::vector<VkSemaphore> render_finished_semaphores;
        std::vector<VkFence> in_flight_fences;
    };
}