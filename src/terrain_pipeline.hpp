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
    glm::vec3 sunDirection;
    float padding1;
    glm::vec3 sunColor;
    float ambientIntensity;
    float hexSize;
    int32_t currentEra;
    float padding2[2];
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

