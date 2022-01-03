cmake_minimum_required(VERSION 3.20)
project(vulkan_sandbox)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(thirdparty/glfw)
add_subdirectory(thirdparty/vulkan)
add_subdirectory(thirdparty/volk)
add_subdirectory(thirdparty/vk-bootstrap)
add_subdirectory(thirdparty/glm)

add_library(vulkan_sandbox_base
        base/application.cpp
        base/vulkan_context.cpp
        )

target_link_libraries(vulkan_sandbox_base PUBLIC glfw)
target_link_libraries(vulkan_sandbox_base PUBLIC volk-lib)
target_link_libraries(vulkan_sandbox_base PUBLIC vk-bootstrap)
target_link_libraries(vulkan_sandbox_base PUBLIC glm::glm)

target_include_directories(vulkan_sandbox_base PUBLIC thirdparty/spdlog)

add_executable(vulkan_sandbox main.cpp )

target_link_libraries(vulkan_sandbox PUBLIC vulkan_sandbox_base)


target_include_directories(vulkan_sandbox PUBLIC .)
