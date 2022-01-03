add_library(vk-bootstrap src/VkBootstrap.cpp)

target_link_libraries(vk-bootstrap PUBLIC vulkan)

target_include_directories(vk-bootstrap PUBLIC src)