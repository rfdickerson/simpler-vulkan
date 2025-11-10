#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

struct Device;
struct Buffer;

struct Image {
    VkImage       image;
    VmaAllocation allocation;
    VkImageView   view;
    VkFormat      format;
    uint32_t      width;
    uint32_t      height;
    uint32_t      mipLevels;
};

// Create an image with VMA
Image createImage(Device& device, uint32_t width, uint32_t height, VkFormat format,
                  VkImageUsageFlags usage, VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_AUTO,
                  uint32_t mipLevels = 1);

// Create an image view for the image
void createImageView(Device& device, Image& image, VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT);

// Destroy image and its resources
void destroyImage(Device& device, Image& image);

// Transition image layout using a command buffer
void transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkFormat format,
                           VkImageLayout oldLayout, VkImageLayout newLayout,
                           uint32_t mipLevels = 1);

// Copy buffer to image
void copyBufferToImage(VkCommandBuffer cmd, VkBuffer buffer, VkImage image,
                       uint32_t width, uint32_t height);

// Create a staging buffer and upload data to an image
// Returns the staging buffer which must be destroyed by the caller after the command buffer is submitted
Buffer uploadImageData(Device& device, VkCommandBuffer cmd, Image& image, 
                       const void* data, size_t dataSize);

