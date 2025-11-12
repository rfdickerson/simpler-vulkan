#pragma once

#include <vulkan/vulkan.h>
#include <vector>

struct Device;
struct Swapchain;

struct TextVertex {
    float pos[2];  // Position
    float uv[2];   // UV coordinates
};

struct TextPushConstants {
    float screenSize[2];
    float textColor[4]; // RGBA, stored as two vec2s in shader
};

struct TextPipeline {
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};

    VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorPool descriptorPool{VK_NULL_HANDLE};
    std::vector<VkDescriptorSet> descriptorSets;

    VkCommandPool commandPool{VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> commandBuffers;
};

// Load shader module from SPIR-V file
VkShaderModule loadShaderModule(Device& device, const char* filepath);

// Create text rendering pipeline
void createTextPipeline(Device& device, Swapchain& swapchain, TextPipeline& pipeline,
                        uint32_t maxDescriptorSets = 8);

// Allocate descriptor set for a given atlas
VkDescriptorSet allocateTextDescriptorSet(Device& device, TextPipeline& pipeline);

// Update descriptor set with atlas texture
void updateTextDescriptorSet(Device& device, VkDescriptorSet descriptorSet,
                             VkImageView atlasView, VkSampler atlasSampler);

// Create command buffers for rendering
void createTextCommandBuffers(Device& device, TextPipeline& pipeline, uint32_t count);

// Cleanup pipeline
void destroyTextPipeline(Device& device, TextPipeline& pipeline);

