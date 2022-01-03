add_library(volk-lib src/volk.c)
target_include_directories(volk-lib PUBLIC src)
target_link_libraries(volk-lib PUBLIC vulkan)