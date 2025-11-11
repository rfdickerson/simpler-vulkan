#include "tree_renderer.hpp"
#include "hex_coord.hpp"
#include <iostream>
#include <cstring>

TreeRenderer::TreeRenderer(Device& device)
    : device(device)
    , vertexBuffer{VK_NULL_HANDLE, nullptr}
    , indexBuffer{VK_NULL_HANDLE, nullptr}
    , instanceBuffer{VK_NULL_HANDLE, nullptr}
    , indexCount(0)
    , rng(std::random_device{}())
{
    generateBoxMesh();
}

TreeRenderer::~TreeRenderer() {
    if (vertexBuffer.buffer != VK_NULL_HANDLE) {
        destroyBuffer(device, vertexBuffer);
    }
    if (indexBuffer.buffer != VK_NULL_HANDLE) {
        destroyBuffer(device, indexBuffer);
    }
    if (instanceBuffer.buffer != VK_NULL_HANDLE) {
        destroyBuffer(device, instanceBuffer);
    }
}

void TreeRenderer::generateBoxMesh() {
    // Simple box mesh (1x2x1 for tree-like proportions: width x height x depth)
    float halfWidth = 0.15f;
    float height = 0.6f;
    float halfDepth = 0.15f;
    
    std::vector<BoxVertex> vertices = {
        // Bottom face (y = 0)
        BoxVertex(glm::vec3(-halfWidth, 0.0f, -halfDepth)),
        BoxVertex(glm::vec3(halfWidth, 0.0f, -halfDepth)),
        BoxVertex(glm::vec3(halfWidth, 0.0f, halfDepth)),
        BoxVertex(glm::vec3(-halfWidth, 0.0f, halfDepth)),
        
        // Top face (y = height)
        BoxVertex(glm::vec3(-halfWidth, height, -halfDepth)),
        BoxVertex(glm::vec3(halfWidth, height, -halfDepth)),
        BoxVertex(glm::vec3(halfWidth, height, halfDepth)),
        BoxVertex(glm::vec3(-halfWidth, height, halfDepth)),
    };
    
    std::vector<uint32_t> indices = {
        // Bottom face
        0, 2, 1, 0, 3, 2,
        // Top face
        4, 5, 6, 4, 6, 7,
        // Front face
        0, 1, 5, 0, 5, 4,
        // Back face
        2, 3, 7, 2, 7, 6,
        // Left face
        3, 0, 4, 3, 4, 7,
        // Right face
        1, 2, 6, 1, 6, 5,
    };
    
    indexCount = static_cast<uint32_t>(indices.size());
    
    // Create vertex buffer
    VkDeviceSize vertexBufferSize = sizeof(BoxVertex) * vertices.size();
    
    VkBufferCreateInfo vertexBufferInfo{};
    vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertexBufferInfo.size = vertexBufferSize;
    vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vertexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VmaAllocationCreateInfo vertexAllocInfo{};
    vertexAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    vertexAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    
    VmaAllocationInfo vertexAllocInfoResult;
    if (vmaCreateBuffer(device.allocator, &vertexBufferInfo, &vertexAllocInfo, &vertexBuffer.buffer, 
                       &vertexBuffer.allocation, &vertexAllocInfoResult) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create tree vertex buffer!");
    }
    
    memcpy(vertexAllocInfoResult.pMappedData, vertices.data(), vertexBufferSize);
    
    // Create index buffer
    VkDeviceSize indexBufferSize = sizeof(uint32_t) * indices.size();
    
    VkBufferCreateInfo indexBufferInfo{};
    indexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    indexBufferInfo.size = indexBufferSize;
    indexBufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    indexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VmaAllocationCreateInfo indexAllocInfo{};
    indexAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    indexAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    
    VmaAllocationInfo indexAllocInfoResult;
    if (vmaCreateBuffer(device.allocator, &indexBufferInfo, &indexAllocInfo, &indexBuffer.buffer, 
                       &indexBuffer.allocation, &indexAllocInfoResult) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create tree index buffer!");
    }
    
    memcpy(indexAllocInfoResult.pMappedData, indices.data(), indexBufferSize);
    
    std::cout << "Tree box mesh created: " << vertices.size() << " vertices, " << indices.size() << " indices" << std::endl;
}

void TreeRenderer::generateTrees(const TerrainRenderer& terrainRenderer) {
    instances.clear();
    
    std::uniform_real_distribution<float> offsetDist(-0.4f, 0.4f);
    std::uniform_real_distribution<float> rotationDist(0.0f, glm::two_pi<float>());
    std::uniform_int_distribution<int> countDist(3, 8);
    
    const auto& tiles = terrainRenderer.getTiles();
    float hexSize = terrainRenderer.getRenderParams().hexSize;
    
    for (const auto& [hex, tile] : tiles) {
        // Only place trees on grassland tiles
        if (tile.type != TerrainType::Grassland) {
            continue;
        }
        
        // Place random number of trees on this tile
        int treeCount = countDist(rng);
        glm::vec3 hexCenter = hexToWorld(hex, hexSize);
        hexCenter.y = tile.height;
        
        for (int i = 0; i < treeCount; ++i) {
            // Random offset within hex bounds (simplified as square for now)
            float offsetX = offsetDist(rng) * hexSize;
            float offsetZ = offsetDist(rng) * hexSize;
            
            // Small vertical offset to prevent z-fighting with terrain
            glm::vec3 position = hexCenter + glm::vec3(offsetX, 0.001f, offsetZ);
            float rotation = rotationDist(rng);
            
            instances.emplace_back(position, rotation);
        }
    }
    
    std::cout << "Generated " << instances.size() << " tree instances" << std::endl;
    
    // Upload to GPU
    uploadInstancesToGPU();
}

void TreeRenderer::uploadInstancesToGPU() {
    if (instances.empty()) {
        return;
    }
    
    // Destroy old instance buffer if it exists
    if (instanceBuffer.buffer != VK_NULL_HANDLE) {
        destroyBuffer(device, instanceBuffer);
        instanceBuffer = {VK_NULL_HANDLE, nullptr};
    }
    
    // Create instance buffer
    VkDeviceSize instanceBufferSize = sizeof(TreeInstance) * instances.size();
    
    VkBufferCreateInfo instanceBufferInfo{};
    instanceBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    instanceBufferInfo.size = instanceBufferSize;
    instanceBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    instanceBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VmaAllocationCreateInfo instanceAllocInfo{};
    instanceAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    instanceAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    
    VmaAllocationInfo instanceAllocInfoResult;
    if (vmaCreateBuffer(device.allocator, &instanceBufferInfo, &instanceAllocInfo, &instanceBuffer.buffer, 
                       &instanceBuffer.allocation, &instanceAllocInfoResult) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create tree instance buffer!");
    }
    
    memcpy(instanceAllocInfoResult.pMappedData, instances.data(), instanceBufferSize);
}

void TreeRenderer::render(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout, const glm::mat4& viewProj) {
    if (instances.empty()) {
        return;
    }
    
    // Push constants (just viewProj matrix)
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &viewProj);
    
    // Bind vertex buffer (box geometry)
    VkBuffer vertexBuffers[] = {vertexBuffer.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
    
    // Bind instance buffer
    VkBuffer instanceBuffers[] = {instanceBuffer.buffer};
    VkDeviceSize instanceOffsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 1, 1, instanceBuffers, instanceOffsets);
    
    // Bind index buffer
    vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    
    // Draw instanced
    vkCmdDrawIndexed(cmd, indexCount, static_cast<uint32_t>(instances.size()), 0, 0, 0);
}

