#pragma once

#include <array>
#include <vector>
#include <vulkan/vulkan.h>

#include "device.hpp"
#include "swapchain.hpp"

struct ColoredVertex {
    float pos[2];
    float color[3];
};

struct TrianglePipeline {
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};
};

void createTrianglePipeline(Device& device, Swapchain& swapchain, TrianglePipeline& pipeline);
void destroyTrianglePipeline(Device& device, TrianglePipeline& pipeline);
