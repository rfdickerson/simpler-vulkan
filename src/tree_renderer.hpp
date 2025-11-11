#pragma once

#include <vector>
#include <random>
#include "device.hpp"
#include "buffer.hpp"
#include "camera.hpp"
#include "terrain_renderer.hpp"

// Instance data for each tree
struct TreeInstance {
    glm::vec3 position;
    float rotation;  // Rotation around Y-axis in radians
    
    TreeInstance() : position(0.0f), rotation(0.0f) {}
    TreeInstance(glm::vec3 pos, float rot) : position(pos), rotation(rot) {}
};

// Simple box mesh vertex
struct BoxVertex {
    glm::vec3 position;
    
    BoxVertex() : position(0.0f) {}
    BoxVertex(glm::vec3 pos) : position(pos) {}
};

// Tree renderer - manages instanced rendering of trees on grass tiles
class TreeRenderer {
public:
    TreeRenderer(Device& device);
    ~TreeRenderer();
    
    // Generate tree instances based on terrain
    void generateTrees(const TerrainRenderer& terrainRenderer);
    
    // Render all tree instances
    void render(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout, const glm::mat4& viewProj);
    
    // Get instance count
    uint32_t getInstanceCount() const { return static_cast<uint32_t>(instances.size()); }
    
private:
    Device& device;
    
    // Tree instances
    std::vector<TreeInstance> instances;
    
    // Geometry buffers
    Buffer vertexBuffer;
    Buffer indexBuffer;
    Buffer instanceBuffer;
    
    uint32_t indexCount;
    
    // Random number generator
    std::mt19937 rng;
    
    // Generate box mesh
    void generateBoxMesh();
    
    // Upload instance data to GPU
    void uploadInstancesToGPU();
};

