# CMakeList.txt : CMake project for CMakeProject7, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.25)

project ("CMakeProject7")

add_subdirectory(extern/VulkanMemoryAllocator)

find_package(Vulkan REQUIRED)


# Add source to this project's executable.
add_executable (CMakeProject7 "CMakeProject7.cpp" "vma_impl.cpp")

target_link_libraries(CMakeProject7 PRIVATE Vulkan::Vulkan GPUOpen::VulkanMemoryAllocator)
