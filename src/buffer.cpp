#include "buffer.hpp"

#include "device.hpp"

#include <stdexcept>

// Create a Storage Buffer (SSBO) using VMA. The buffer is host-accessible for easy CPU writes.
Buffer CreateSsboBuffer(Device& device, VkDeviceSize size) {
    Buffer buffer{};

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size  = size;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT; // optional, useful for uploads/downloads
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    // Make it host-visible so the CPU can write into the SSBO easily.
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    VkResult res =
        vmaCreateBuffer(device.allocator, &bufferInfo, &allocInfo, &buffer.buffer, &buffer.allocation, nullptr);
    if (res != VK_SUCCESS) {
        throw std::runtime_error("failed to create SSBO buffer with VMA!");
    }

    return buffer;
}

// Create a Vertex Buffer using VMA. The buffer is host-accessible for easy CPU writes.
Buffer CreateVertexBuffer(Device& device, VkDeviceSize size) {
    Buffer buffer{};

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size  = size;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    // Make it host-visible so the CPU can write into the vertex buffer easily.
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    VkResult res =
        vmaCreateBuffer(device.allocator, &bufferInfo, &allocInfo, &buffer.buffer, &buffer.allocation, nullptr);
    if (res != VK_SUCCESS) {
        throw std::runtime_error("failed to create vertex buffer with VMA!");
    }

    return buffer;
}

void destroyBuffer(Device& device, Buffer& buffer) {
    if (buffer.buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(device.allocator, buffer.buffer, buffer.allocation);
        buffer.buffer     = VK_NULL_HANDLE;
        buffer.allocation = nullptr;
    }
}