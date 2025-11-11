#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "image.hpp"

struct Device;
struct Window;

struct SwapchainImage {
    VkImage image;
    VkImageView view;
};

struct Swapchain {
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat format;
    VkExtent2D extent;
    std::vector<SwapchainImage> images;

    // MSAA color target
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    Image msaaColor;
    bool msaaNeedsTransition = false;
    
    // Depth buffer
    Image depthImage;
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
    
    uint32_t currentImageIndex = 0;
    
    // Synchronization
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
};

// Create surface for GLFW window
VkSurfaceKHR createSurface(VkInstance instance, Window& window);

// Create swapchain
void createSwapchain(Device& device, VkSurfaceKHR surface, Window& window, Swapchain& swapchain);

// Recreate swapchain (for window resize)
void recreateSwapchain(Device& device, VkSurfaceKHR surface, Window& window, Swapchain& swapchain);

// Cleanup swapchain
void cleanupSwapchain(Device& device, Swapchain& swapchain);

// Destroy surface
void destroySurface(VkInstance instance, VkSurfaceKHR surface);

// Acquire next image
bool acquireNextImage(Device& device, Swapchain& swapchain);

// Present image
bool presentImage(Device& device, VkSurfaceKHR surface, Swapchain& swapchain);

