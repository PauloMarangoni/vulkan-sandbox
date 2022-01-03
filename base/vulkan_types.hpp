#pragma once

#include "volk.h"

namespace vk_sandbox {

    struct VulkanImage {
        VkFormat image_format{};
        VkImage image{};
        VkImageView image_view{};
        VkSampler sampler{};
        VmaAllocation allocation{};
        VkExtent3D extent{};
    };

}