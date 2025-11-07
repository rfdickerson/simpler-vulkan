#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

struct Device;

struct Buffer {
    VkBuffer      buffer;
    VmaAllocation allocation;
};

Buffer CreateSsboBuffer(Device& device, VkDeviceSize size);
void   destroyBuffer(Device& device, Buffer& buffer);
