#pragma once

#include <vulkan/vulkan.h>
#include "firmament/device.hpp"
#include "firmament/swapchain.hpp"

using namespace firmament;

struct SSAOPushConstants {
    float radius;
    float bias;
    float intensity;
    float padding; // align to 16 bytes
    float invProj[16];
};

struct SSAOPipeline {
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};

    VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorPool descriptorPool{VK_NULL_HANDLE};
    VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
};

void createSSAOPipeline(Device& device, Swapchain& swapchain, SSAOPipeline& pipeline);
void destroySSAOPipeline(Device& device, SSAOPipeline& pipeline);
void updateSSAODepthDescriptor(Device& device, SSAOPipeline& pipeline, Swapchain& swapchain);
