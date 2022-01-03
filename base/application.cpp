#include "application.hpp"

namespace vk_sandbox {

    static void error_callback(int error, const char* description){
        spdlog::error("[GLFW] error: {} ", description);
    }

    Application::Application(const std::string& title) : title(title) {}

    void Application::init() {

        if (!glfwInit()) {
            spdlog::error("Error on initialize GLFW");
            return;
        }

        glfwSetErrorCallback(error_callback);

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        spdlog::info("GLFW initialized successfully");
        context.init_vulkan();
        create_window();
        loop();
    }

    void Application::create_window() {
        window = glfwCreateWindow(800, 600, title.c_str(), NULL, NULL);
        glfwMaximizeWindow(window);
        glfwWaitEvents();
        context.create_swapchain(window, true);
    }

    void Application::loop() {
        while (!glfwWindowShouldClose(window)) {
            this->draw();
            glfwPollEvents();
        }
        glfwTerminate();
    }

    void Application::destroy() {
        context.destroy_vulkan();
    }
}