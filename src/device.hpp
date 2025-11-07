#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

struct Device {
	VkInstance instance;
	VkPhysicalDevice physicalDevice;
	VkDevice device;
	VkQueue queue;
	VkSemaphore timelineSemaphore;

	VmaAllocator allocator;
};

void initDevice(Device& device);
void initVma(Device& device);
void initVulkan(Device& device);
void makeTimelineSemaphore(Device& device);

void cleanupVma(Device& device);
void cleanupVulkan(Device& device);

