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

        images_in_flight.resize(vkb_images.size());
        images.resize(vkb_images.size());

        for (int i = 0; i < vkb_images.size(); ++i) {
            images[i].image_format = vkb_swapchain.image_format;
            images[i].image = vkb_images[i];
            images[i].image_view = vkb_image_views[i];
        }
        cmds.resize(vkb_images.size());
        VkCommandBufferAllocateInfo alloc_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandPool = command_pool;
        alloc_info.commandBufferCount = cmds.size();
        vkAllocateCommandBuffers(device, &alloc_info, cmds.data());

        VkSemaphoreCreateInfo semaphore_info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        image_available_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        render_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);

        for (auto i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkCreateSemaphore(device, &semaphore_info, nullptr, &image_available_semaphores[i]);
            vkCreateSemaphore(device, &semaphore_info, nullptr, &render_finished_semaphores[i]);
            vkCreateFence(device, &fence_info, nullptr, &in_flight_fences[i]);
        }

        //render pass
        VkAttachmentDescription attachment_description{};
        VkAttachmentReference attachment_reference{};

        attachment_description.format = images[0].image_format;
        attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment_description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment_description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment_description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        attachment_reference.attachment = 0;
        attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription sub_pass = {};
        sub_pass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        sub_pass.colorAttachmentCount = 1;
        sub_pass.pColorAttachments = &attachment_reference;

        VkRenderPassCreateInfo render_pass_info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        render_pass_info.attachmentCount = 1;
        render_pass_info.pAttachments = &attachment_description;
        render_pass_info.subpassCount = 1;
        render_pass_info.pSubpasses = &sub_pass;

        vkCreateRenderPass(device, &render_pass_info, nullptr, &swapchain_renderpass);
        swapchain_framebuffers.resize(images.size());


        for (int i = 0; i < images.size(); ++i) {
            VkFramebufferCreateInfo fb_info = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            fb_info.renderPass = swapchain_renderpass;
            fb_info.width = framebuffer_size.x;
            fb_info.height = framebuffer_size.y;
            fb_info.layers = 1;
            fb_info.attachmentCount = 1;
            fb_info.pAttachments = &images[i].image_view;
            vkCreateFramebuffer(device, &fb_info, nullptr, &swapchain_framebuffers[i]);
        }
        spdlog::info("[VulkanContext] swapchain created successfully");

    }

    bool VulkanContext::begin_frame(glm::ivec2 size) {

        auto result = vkAcquireNextImageKHR(device,
                                            swapchain_khr,
                                            UINT64_MAX,
                                            image_available_semaphores[current_frame],
                                            VK_NULL_HANDLE,
                                            &image_index);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            //TODO - recreate_swapchain();
            return false;
        } else if (result != VK_SUCCESS) {
            spdlog::error("[VulkanRendererContext] failed to acquire swap chain image!");
            return false;
        }

        VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        vkBeginCommandBuffer(cmds[image_index], &begin_info);

        begin_render_pass(swapchain_renderpass, swapchain_framebuffers[image_index], size);
        return true;
    }

    void VulkanContext::end_frame() {

        //temporary
        vkCmdEndRenderPass(cmds[image_index]);

        vkEndCommandBuffer(cmds[image_index]);
        vkWaitForFences(device, 1, &in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);

        if (images_in_flight[image_index] != VK_NULL_HANDLE) {
            vkWaitForFences(device, 1, &images_in_flight[image_index], VK_TRUE, UINT64_MAX);
        }

        images_in_flight[image_index] = in_flight_fences[current_frame];

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore wait_semaphores[] = {image_available_semaphores[current_frame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = wait_semaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmds[image_index];

        VkSemaphore signalSemaphores[] = {render_finished_semaphores[current_frame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        vkResetFences(device, 1, &in_flight_fences[current_frame]);

        auto result = vkQueueSubmit(graphics_queue, 1, &submitInfo, in_flight_fences[current_frame]);

        if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        VkSemaphore signal_semaphores[] = {render_finished_semaphores[current_frame]};

        VkPresentInfoKHR present_info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = signal_semaphores;

        VkSwapchainKHR swap_chains[] = {swapchain_khr};
        present_info.swapchainCount = 1;
        present_info.pSwapchains = swap_chains;

        present_info.pImageIndices = &image_index;

        result = vkQueuePresentKHR(present_queue, &present_info);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            //TODO recreate_swapchain();
        } else if (result != VK_SUCCESS) {
            spdlog::error("[VulkanRendererContext] failed to present swap chain image!");
        }
    }


    VulkanRenderTarget VulkanContext::create_render_target() {


        return VulkanRenderTarget();
    }

    void VulkanContext::begin_render_pass(VkRenderPass render_pass, VkFramebuffer framebuffer, glm::ivec2 size) {

        VkRenderPassBeginInfo render_pass_info{};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass = render_pass;
        render_pass_info.framebuffer = framebuffer;
        render_pass_info.renderArea.offset = {0, 0};
        render_pass_info.renderArea.extent = {static_cast<uint32_t>(size.x), static_cast<uint32_t>(size.y)};

        VkClearValue clear_value{};
        clear_value.color = {0.2f, 0.3f, 0.3f, 1.0f};

        render_pass_info.clearValueCount = 1;
        render_pass_info.pClearValues = &clear_value;

        vkCmdBeginRenderPass(cmds[image_index], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport vk_viewport;
        vk_viewport.x = 0;
        vk_viewport.y = size.y;
        vk_viewport.width = size.x;
        vk_viewport.height = -size.y;

        vk_viewport.minDepth = 0.0f;
        vk_viewport.maxDepth = 1.f;
        vkCmdSetViewport(cmds[image_index], 0, 1, &vk_viewport);

        VkRect2D rect_2d;
        rect_2d.offset.x = 0;
        rect_2d.offset.y = 0;
        rect_2d.extent.width = static_cast<uint32_t>(size.x);
        rect_2d.extent.height = static_cast<uint32_t>(size.y);
        vkCmdSetScissor(cmds[image_index], 0, 1, &rect_2d);
    }

    void VulkanContext::destroy_vulkan() {
        vkDeviceWaitIdle(device);

        for (auto i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(device, image_available_semaphores[i], nullptr);
            vkDestroySemaphore(device, render_finished_semaphores[i], nullptr);
            vkDestroyFence(device, in_flight_fences[i], nullptr);
        }

        for (int i = 0; i < swapchain_framebuffers.size(); ++i) {
            vkDestroyFramebuffer(device, swapchain_framebuffers[i], nullptr);
        }

        vkDestroyRenderPass(device, swapchain_renderpass, nullptr);

        for (int i = 0; i < images.size(); ++i) {
            vkDestroyImageView(device, images[i].image_view, nullptr);
        }

        vkDestroySwapchainKHR(device, swapchain_khr, nullptr);
        vkDestroySurfaceKHR(instance, surface_khr, nullptr);

        vkFreeCommandBuffers(device, command_pool, 1, &temporary_command_buffer);
        vkDestroyCommandPool(device, command_pool, nullptr);
        vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
        vmaDestroyAllocator(allocator);
        vkDestroyDevice(device, nullptr);
        vkb::destroy_debug_utils_messenger(instance, debug_messenger);
        vkDestroyInstance(this->instance, nullptr);
    }
}
