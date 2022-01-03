#pragma once

#include "volk.h"
#include <vector>

namespace vk_sandbox {

    const int MAX_FRAMES_IN_FLIGHT = 2;

    struct VulkanImage {
        VkFormat image_format{};
        VkImage image{};
        VkImageView image_view{};
        VkSampler sampler{};
        VmaAllocation allocation{};
        VkExtent3D extent{};
    };

    struct VulkanRenderTarget{
        VkRenderPass vk_render_pass;
        std::vector<VkFramebuffer> framebuffers;
    };

}