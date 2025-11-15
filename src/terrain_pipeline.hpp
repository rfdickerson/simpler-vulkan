#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include "device.hpp"
#include "swapchain.hpp"

// Push constants for terrain rendering
struct TerrainPushConstants {
    glm::mat4 viewProj;
    glm::vec3 cameraPos;
    float time;
};

// Uniform buffer for terrain parameters
struct TerrainParamsUBO {
    glm::vec4 sunDirSnow;        // xyz = sun direction, w = snow line threshold (0-1 in normalized elevation)
    glm::vec4 sunColorAmbient;   // xyz = sun color, w = ambient intensity
    glm::vec4 fieldScales;       // x = hex size, y = moisture noise scale, z = temperature noise scale, w = biome noise scale
    glm::vec4 fieldBias;         // x = temperature lapse rate, y = moisture bias, z = slope rock strength, w = river rock bias
    glm::vec4 elevationInfo;     // x = min elevation, y = max elevation, z = detail noise scale, w = unused/padding
    glm::ivec4 eraInfo;          // x = current era index, remaining components unused
};

// Terrain pipeline structure
struct TerrainPipeline {
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;
    VkPipeline depthOnlyPipeline; // Depth-only variant for prepass
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;
    
    // Uniform buffer
    VkBuffer uniformBuffer;
    VmaAllocation uniformAllocation;
    void* uniformMapped;
    
    // Command buffers (one per frame in flight)
    std::vector<VkCommandBuffer> commandBuffers;
    VkCommandPool commandPool;
};

// Pipeline creation and management
void createTerrainPipeline(Device& device, Swapchain& swapchain, TerrainPipeline& pipeline);
void destroyTerrainPipeline(Device& device, TerrainPipeline& pipeline);

// Command buffer management
void createTerrainCommandBuffers(Device& device, TerrainPipeline& pipeline, uint32_t count);

// Update terrain parameters
void updateTerrainParams(TerrainPipeline& pipeline, const TerrainParamsUBO& params);

// Update SSAO texture binding
void updateTerrainSsaoDescriptor(Device& device, TerrainPipeline& pipeline, VkImageView ssaoView, VkSampler ssaoSampler);

