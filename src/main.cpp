#include <vulkan/vulkan.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstdint>


#include "device.hpp"
#include "buffer.hpp"
#include "window.hpp"
#include "text.hpp"


int main()
{
    Window window;
    window.InitGLFW();
    window.CreateWindow(800, 600, "Engine");

	Device device;
	initVulkan(device);
	initDevice(device);
	initVma(device);
	makeTimelineSemaphore(device);

	HbShaper shaper("../assets/fonts/EBGaramond-Regular.ttf", 48);
    auto     glyphs = shaper.shape_utf8("Hello world");

	// Example: create a 4KB SSBO
	Buffer ssbo = CreateSsboBuffer(device, 4 * 1024);
	std::cout << "SSBO created." << std::endl;

	while (!window.shouldClose()) {
        window.pollEvents();
	}

	// Cleanup
	destroyBuffer(device, ssbo);
	vkDestroySemaphore(device.device, device.timelineSemaphore, nullptr);
	cleanupVma(device);
	vkDestroyDevice(device.device, nullptr);
	cleanupVulkan(device);

	return 0;
}
