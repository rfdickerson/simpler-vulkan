#pragma once

#include <vulkan/vulkan.h>
#include "firmament/device.hpp"
#include "firmament/swapchain.hpp"

using namespace firmament;

struct TiltShiftPushConstants {
    float cosAngle;
    float sinAngle;
    float focusCenter;      // [0,1] in rotated space
    float bandHalfWidth;    // [0,1] half-width of sharp band
    float blurScale;        // blur scale per unit distance outside band
    float maxRadiusPixels;  // clamp radius in pixels
    float resolution[2];    // screen size in pixels
    float padding;          // align to 16 bytes
};

struct TiltShiftPipeline {
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};

    VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorPool descriptorPool{VK_NULL_HANDLE};
    VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
};

void createTiltShiftPipeline(Device& device, Swapchain& swapchain, TiltShiftPipeline& pipeline);
void destroyTiltShiftPipeline(Device& device, TiltShiftPipeline& pipeline);
void updateTiltShiftDescriptors(Device& device, TiltShiftPipeline& pipeline, Swapchain& swapchain);


