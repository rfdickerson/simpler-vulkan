#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include "firmament/device.hpp"
#include "firmament/swapchain.hpp"

using namespace firmament;

// Push constants for tree rendering
struct TreePushConstants {
    glm::mat4 viewProj;
};

// Tree pipeline structure
struct TreePipeline {
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;
    VkPipeline depthOnlyPipeline; // Depth-only variant for prepass
};

// Pipeline creation and management
void createTreePipeline(Device& device, Swapchain& swapchain, TreePipeline& pipeline, VkFormat depthFormat);
void destroyTreePipeline(Device& device, TreePipeline& pipeline);

