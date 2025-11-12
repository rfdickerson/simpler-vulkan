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
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;
    
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;
    
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;
};

// Create text rendering pipeline
void createTextPipeline(Device& device, Swapchain& swapchain, TextPipeline& pipeline);

// Update descriptor set with atlas texture
void updateTextDescriptorSet(Device& device, TextPipeline& pipeline, 
                             VkImageView atlasView, VkSampler atlasSampler);

// Create command buffers for rendering
void createTextCommandBuffers(Device& device, TextPipeline& pipeline, uint32_t count);

// Cleanup pipeline
void destroyTextPipeline(Device& device, TextPipeline& pipeline);

