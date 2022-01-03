#pragma once

#include <string>
#include "spdlog/spdlog.h"

#define GLFW_INCLUDE_VULKAN
#define VK_NO_PROTOTYPES
#include <GLFW/glfw3.h>
#include "vulkan_context.hpp"

namespace vk_sandbox {

    class Application {
    public:
        Application(const std::string& title);

        virtual void draw() = 0;

        void init();
        void destroy();
    private:
        GLFWwindow *window;
        VulkanContext context;

        std::string title;

        void create_window();

        void loop();
    };

}