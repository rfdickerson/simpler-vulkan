# CMakeList.txt : CMake project for CMakeProject7, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.25)

project ("CMakeProject7")

add_subdirectory(extern/VulkanMemoryAllocator)
add_subdirectory(extern/glfw)

find_package(Vulkan REQUIRED)

add_compile_definitions(GLFW_INCLUDE_NONE)


# Add source to this project's executable.
add_executable (CMakeProject7 "src/main.cpp" "src/vma_impl.cpp" "src/device.cpp" "src/buffer.cpp" "src/window.cpp")

target_link_libraries(CMakeProject7 PRIVATE Vulkan::Vulkan GPUOpen::VulkanMemoryAllocator glfw)

target_precompile_headers(CMakeProject7 PRIVATE src/pch.hpp)
