#include "swapchain.hpp"
#include "device.hpp"
#include "window.hpp"
#include "image.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>

VkSurfaceKHR createSurface(VkInstance instance, Window& window) {
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(instance, window.window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface!");
    }
    return surface;
}

static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& format : availableFormats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    return availableFormats[0];
}

static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availableModes) {
    for (const auto& mode : availableModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, Window& window) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        VkExtent2D actualExtent = {
            static_cast<uint32_t>(window.width),
            static_cast<uint32_t>(window.height)
        };

        actualExtent.width = std::clamp(actualExtent.width,
                                       capabilities.minImageExtent.width,
                                       capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height,
                                        capabilities.minImageExtent.height,
                                        capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

static VkSampleCountFlagBits chooseMsaaSamples(VkPhysicalDevice physicalDevice, VkSampleCountFlagBits desired) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    VkSampleCountFlags colorCounts = props.limits.framebufferColorSampleCounts;
    // Prefer desired, otherwise fall back
    if (colorCounts & desired) return desired;
    if (colorCounts & VK_SAMPLE_COUNT_4_BIT) return VK_SAMPLE_COUNT_4_BIT;
    if (colorCounts & VK_SAMPLE_COUNT_2_BIT) return VK_SAMPLE_COUNT_2_BIT;
    return VK_SAMPLE_COUNT_1_BIT;
}

void createSwapchain(Device& device, VkSurfaceKHR surface, Window& window, Swapchain& swapchain) {
    // Query swap chain support
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.physicalDevice, surface, &capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.physicalDevice, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.physicalDevice, surface, &formatCount, formats.data());

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device.physicalDevice, surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device.physicalDevice, surface, &presentModeCount, presentModes.data());

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(presentModes);
    VkExtent2D extent = chooseSwapExtent(capabilities, window);

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device.device, &createInfo, nullptr, &swapchain.swapchain) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swap chain!");
    }

    // Retrieve swap chain images
    vkGetSwapchainImagesKHR(device.device, swapchain.swapchain, &imageCount, nullptr);
    std::vector<VkImage> images(imageCount);
    vkGetSwapchainImagesKHR(device.device, swapchain.swapchain, &imageCount, images.data());

    swapchain.format = surfaceFormat.format;
    swapchain.extent = extent;
    swapchain.images.resize(imageCount);

    // Create image views
    for (size_t i = 0; i < imageCount; i++) {
        swapchain.images[i].image = images[i];

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = surfaceFormat.format;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device.device, &viewInfo, nullptr, &swapchain.images[i].view) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image views!");
        }
    }

    // MSAA setup: create a single transient color image with 4x if supported
    swapchain.msaaSamples = chooseMsaaSamples(device.physicalDevice, VK_SAMPLE_COUNT_4_BIT);
    if (swapchain.msaaSamples != VK_SAMPLE_COUNT_1_BIT) {
        swapchain.msaaColor = createImage(device, extent.width, extent.height, swapchain.format,
                                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                          VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                                          1, swapchain.msaaSamples);
        createImageView(device, swapchain.msaaColor, VK_IMAGE_ASPECT_COLOR_BIT);
    } else {
        swapchain.msaaColor = {}; // zero-init
    }

    // Create depth buffer (also sampleable for SSAO)
    swapchain.depthImage = createImage(device, extent.width, extent.height, swapchain.depthFormat,
                                       VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                       VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                                       1, swapchain.msaaSamples);
    createImageView(device, swapchain.depthImage, VK_IMAGE_ASPECT_DEPTH_BIT);

    // Create SSAO target (single-sampled color, sampled later)
    swapchain.ssaoImage = createImage(device, extent.width, extent.height, swapchain.ssaoFormat,
                                      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                                      1, VK_SAMPLE_COUNT_1_BIT);
    createImageView(device, swapchain.ssaoImage, VK_IMAGE_ASPECT_COLOR_BIT);

    // Create scene color (single-sample) for post-processing
    swapchain.sceneColor = createImage(device, extent.width, extent.height, swapchain.format,
                                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                       VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                                       1, VK_SAMPLE_COUNT_1_BIT);
    createImageView(device, swapchain.sceneColor, VK_IMAGE_ASPECT_COLOR_BIT);

    // Create resolved depth (single-sample) for SSAO sampling
    swapchain.depthResolved = createImage(device, extent.width, extent.height, swapchain.depthFormat,
                                          VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                          VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                                          1, VK_SAMPLE_COUNT_1_BIT);
    createImageView(device, swapchain.depthResolved, VK_IMAGE_ASPECT_DEPTH_BIT);

    // Create SSAO sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    if (vkCreateSampler(device.device, &samplerInfo, nullptr, &swapchain.ssaoSampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create SSAO sampler!");
    }

    // Create synchronization objects
    // imageAvailable: one per frame-in-flight (we don't know which image we'll get before acquire)
    swapchain.imageAvailableSemaphores.resize(Swapchain::MAX_FRAMES_IN_FLIGHT);
    // renderFinished: one per swapchain image (recommended by validation layers to avoid reuse)
    swapchain.renderFinishedSemaphores.resize(imageCount);
    // timeline values: one per frame-in-flight to wait before reuse
    swapchain.frameTimelineValues.clear();
    swapchain.frameTimelineValues.resize(Swapchain::MAX_FRAMES_IN_FLIGHT, 0ull);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    // Create imageAvailable semaphores (per frame-in-flight)
    for (size_t i = 0; i < Swapchain::MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device.device, &semaphoreInfo, nullptr, &swapchain.imageAvailableSemaphores[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create imageAvailable semaphores!");
        }
    }

    // Create renderFinished semaphores (per swapchain image)
    for (size_t i = 0; i < imageCount; i++) {
        if (vkCreateSemaphore(device.device, &semaphoreInfo, nullptr, &swapchain.renderFinishedSemaphores[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create renderFinished semaphores!");
        }
    }

    std::cout << "Swapchain created: " << extent.width << "x" << extent.height
              << " with " << imageCount << " images" << std::endl;
}

void cleanupSwapchain(Device& device, Swapchain& swapchain) {
    // Destroy MSAA color target if present
    if (swapchain.msaaColor.image != VK_NULL_HANDLE) {
        destroyImage(device, swapchain.msaaColor);
        swapchain.msaaColor = {};
    }
    
    // Destroy depth buffer
    if (swapchain.depthImage.image != VK_NULL_HANDLE) {
        destroyImage(device, swapchain.depthImage);
        swapchain.depthImage = {};
    }

    // Destroy resolved depth
    if (swapchain.depthResolved.image != VK_NULL_HANDLE) {
        destroyImage(device, swapchain.depthResolved);
        swapchain.depthResolved = {};
    }

    // Destroy scene color
    if (swapchain.sceneColor.image != VK_NULL_HANDLE) {
        destroyImage(device, swapchain.sceneColor);
        swapchain.sceneColor = {};
    }

    // Destroy SSAO target and sampler
    if (swapchain.ssaoImage.image != VK_NULL_HANDLE) {
        destroyImage(device, swapchain.ssaoImage);
        swapchain.ssaoImage = {};
    }
    if (swapchain.ssaoSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device.device, swapchain.ssaoSampler, nullptr);
        swapchain.ssaoSampler = VK_NULL_HANDLE;
    }
    
    for (auto& img : swapchain.images) {
        if (img.view != VK_NULL_HANDLE) {
            vkDestroyImageView(device.device, img.view, nullptr);
        }
    }
    swapchain.images.clear();

    if (swapchain.swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device.device, swapchain.swapchain, nullptr);
        swapchain.swapchain = VK_NULL_HANDLE;
    }

    // Don't destroy sync objects here - they're reused
}

void recreateSwapchain(Device& device, VkSurfaceKHR surface, Window& window, Swapchain& swapchain) {
    vkDeviceWaitIdle(device.device);

    // Destroy old synchronization objects before cleanupSwapchain
    for (size_t i = 0; i < swapchain.imageAvailableSemaphores.size(); i++) {
        if (swapchain.imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(device.device, swapchain.imageAvailableSemaphores[i], nullptr);
        }
    }
    for (size_t i = 0; i < swapchain.renderFinishedSemaphores.size(); i++) {
        if (swapchain.renderFinishedSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(device.device, swapchain.renderFinishedSemaphores[i], nullptr);
        }
    }

    cleanupSwapchain(device, swapchain);

    // Recreate with old swapchain handle
    VkSwapchainKHR oldSwapchain = swapchain.swapchain;
    createSwapchain(device, surface, window, swapchain);
}

void destroySurface(VkInstance instance, VkSurfaceKHR surface) {
    if (surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
    }
}

bool acquireNextImage(Device& device, Swapchain& swapchain) {
    // Wait on the timeline semaphore for this frame if previously used
    uint64_t waitValue = swapchain.frameTimelineValues[swapchain.currentFrame];
    if (waitValue != 0ull) {
        VkSemaphore waitSem = device.timelineSemaphore;
        VkSemaphoreWaitInfo waitInfo{};
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        waitInfo.semaphoreCount = 1;
        waitInfo.pSemaphores = &waitSem;
        waitInfo.pValues = &waitValue;
        vkWaitSemaphores(device.device, &waitInfo, UINT64_MAX);
    }

    VkResult result = vkAcquireNextImageKHR(device.device, swapchain.swapchain, UINT64_MAX,
                                           swapchain.imageAvailableSemaphores[swapchain.currentFrame],
                                           VK_NULL_HANDLE, &swapchain.currentImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        return false; // Need to recreate swapchain
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    return true;
}

bool presentImage(Device& device, VkSurfaceKHR surface, Swapchain& swapchain) {
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &swapchain.renderFinishedSemaphores[swapchain.currentImageIndex];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain.swapchain;
    presentInfo.pImageIndices = &swapchain.currentImageIndex;

    VkResult result = vkQueuePresentKHR(device.queue, &presentInfo);

    swapchain.currentFrame = (swapchain.currentFrame + 1) % Swapchain::MAX_FRAMES_IN_FLIGHT;

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        return false; // Need to recreate swapchain
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swap chain image!");
    }

    return true;
}

