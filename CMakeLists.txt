cmake_minimum_required (VERSION 3.25)

project ("HexTerrainRenderer" LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(harfBuzz CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Vulkan REQUIRED)

add_compile_definitions(GLFW_INCLUDE_NONE)

# Firmament Engine Library
add_library(firmament STATIC
    "src/firmament/vma_impl.cpp"
    "src/firmament/device.cpp"
    "src/firmament/buffer.cpp"
    "src/firmament/window.cpp"
    "src/firmament/image.cpp"
    "src/firmament/glyph_atlas.cpp"
    "src/firmament/swapchain.cpp"
    "src/firmament/text_pipeline.cpp"
    "src/firmament/render_graph.cpp"
)

target_include_directories(firmament PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/firmament
)

target_link_libraries(firmament PUBLIC
    Vulkan::Vulkan
    GPUOpen::VulkanMemoryAllocator
    glfw
    harfbuzz::harfbuzz
    Freetype::Freetype
    glm::glm
)

target_precompile_headers(firmament PUBLIC src/firmament/pch.hpp)

# Game Executable
add_executable(HexTerrainRenderer
    "src/main.cpp"
    "src/terrain_pipeline.cpp"
    "src/terrain_renderer.cpp"
    "src/tree_pipeline.cpp"
    "src/tree_renderer.cpp"
    "src/map_builder.cpp"
    "src/ssao_pipeline.cpp"
    "src/tiltshift_pipeline.cpp"
)

target_include_directories(HexTerrainRenderer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(HexTerrainRenderer PRIVATE
    firmament
)
