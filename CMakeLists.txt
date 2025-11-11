
cmake_minimum_required (VERSION 3.25)

project ("CMakeProject7" LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)


find_package(harfBuzz CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

find_package(Vulkan REQUIRED)

add_compile_definitions(GLFW_INCLUDE_NONE)

# Add source to this project's executable.
add_executable (CMakeProject7 "src/main.cpp" "src/vma_impl.cpp" "src/device.cpp" "src/buffer.cpp" "src/window.cpp" "src/image.cpp" "src/glyph_atlas.cpp" "src/swapchain.cpp" "src/text_pipeline.cpp" "src/terrain_pipeline.cpp" "src/terrain_renderer.cpp" "src/tree_pipeline.cpp" "src/tree_renderer.cpp" "src/map_builder.cpp" "src/render_graph.cpp")

target_link_libraries(CMakeProject7 PRIVATE Vulkan::Vulkan GPUOpen::VulkanMemoryAllocator glfw harfbuzz::harfbuzz Freetype::Freetype glm::glm)

target_precompile_headers(CMakeProject7 PRIVATE src/pch.hpp)
