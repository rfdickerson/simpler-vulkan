#include <vulkan/vulkan.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstdint>


#include "device.hpp"
#include "buffer.hpp"
#include "window.hpp"


int main()
{
    Window window;
    window.InitGLFW();

	Device device;
	initVulkan(device);
	initDevice(device);
	initVma(device);
	makeTimelineSemaphore(device);

	// Example: create a 4KB SSBO
	Buffer ssbo = CreateSsboBuffer(device, 4 * 1024);
	std::cout << "SSBO created." << std::endl;

	// Cleanup
	destroyBuffer(device, ssbo);
	vkDestroySemaphore(device.device, device.timelineSemaphore, nullptr);
	cleanupVma(device);
	vkDestroyDevice(device.device, nullptr);
	cleanupVulkan(device);

	return 0;
}
