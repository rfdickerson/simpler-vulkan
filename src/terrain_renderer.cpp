#include "terrain_renderer.hpp"
#include <iostream>

TerrainRenderer::TerrainRenderer(Device& device, float hexSize)
    : device(device)
    , hexSize(hexSize)
    , meshDirty(true)
{
    renderParams.hexSize = hexSize;
}

TerrainRenderer::~TerrainRenderer() {
    if (vertexBuffer.buffer != VK_NULL_HANDLE) {
        destroyBuffer(device, vertexBuffer);
    }
    if (indexBuffer.buffer != VK_NULL_HANDLE) {
        destroyBuffer(device, indexBuffer);
    }
}

void TerrainRenderer::initializeRectangularGrid(int width, int height) {
    tiles.clear();
    tileOrder.clear();
    
    // Generate hexes in a rectangular pattern
    for (int r = 0; r < height; ++r) {
        int rOffset = r / 2;
        for (int q = -rOffset; q < width - rOffset; ++q) {
            HexCoord hex(q, r);
            tileOrder.push_back(hex);
            
            TerrainTile tile;
            tile.type = TerrainType::Grassland; // Default terrain
            tile.height = 0.0f;
            tile.explored = 255; // Fully explored for now
            tile.visible = 255;
            
            tiles[hex] = tile;
        }
    }
    
    meshDirty = true;
    std::cout << "Initialized rectangular grid: " << width << "x" << height 
              << " (" << tiles.size() << " tiles)" << std::endl;
}

void TerrainRenderer::initializeRadialGrid(const HexCoord& center, int radius) {
    tiles.clear();
    tileOrder.clear();
    
    std::vector<HexCoord> hexes = hexesInRadius(center, radius);
    
    for (const auto& hex : hexes) {
        tileOrder.push_back(hex);
        
        TerrainTile tile;
        
        // Create some variation based on distance from center
        int dist = hexDistance(center, hex);
        if (dist == 0) {
            tile.type = TerrainType::Plains;
        } else if (dist < radius / 3) {
            tile.type = TerrainType::Grassland;
        } else if (dist < 2 * radius / 3) {
            tile.type = TerrainType::Forest;
        } else {
            tile.type = TerrainType::Hills;
        }
        
        tile.height = dist * 0.1f; // Slight elevation increase
        tile.explored = 255;
        tile.visible = 255;
        
        tiles[hex] = tile;
    }
    
    meshDirty = true;
    std::cout << "Initialized radial grid: radius " << radius 
              << " (" << tiles.size() << " tiles)" << std::endl;
}

void TerrainRenderer::setTerrainType(const HexCoord& hex, TerrainType type) {
    auto it = tiles.find(hex);
    if (it != tiles.end()) {
        it->second.type = type;
        meshDirty = true;
    }
}

void TerrainRenderer::setTerrainHeight(const HexCoord& hex, float height) {
    auto it = tiles.find(hex);
    if (it != tiles.end()) {
        it->second.height = height;
        meshDirty = true;
    }
}

void TerrainRenderer::rebuildMesh() {
    if (!meshDirty) return;
    
    // Generate mesh for all tiles
    mesh = HexMesh::generateHexGrid(tileOrder, hexSize, 
        [this](const HexCoord& hex) -> float {
            auto it = tiles.find(hex);
            return (it != tiles.end()) ? it->second.height : 0.0f;
        });
    
    // Upload to GPU
    uploadMeshToGPU();
    
    meshDirty = false;
    std::cout << "Rebuilt terrain mesh: " << mesh.vertices.size() 
              << " vertices, " << mesh.indices.size() << " indices" << std::endl;
}

void TerrainRenderer::updateRenderParams(const Camera& camera, float time) {
    renderParams.time = time;
    // Update other params as needed (sun direction, era, etc.)
}

void TerrainRenderer::uploadMeshToGPU() {
    // Destroy old buffers if they exist
    if (vertexBuffer.buffer != VK_NULL_HANDLE) {
        destroyBuffer(device, vertexBuffer);
    }
    if (indexBuffer.buffer != VK_NULL_HANDLE) {
        destroyBuffer(device, indexBuffer);
    }
    
    // Create vertex buffer
    VkDeviceSize vertexBufferSize = sizeof(TerrainVertex) * mesh.vertices.size();
    vertexBuffer = CreateVertexBuffer(device, vertexBufferSize);
    
    // Upload vertex data
    void* data;
    vmaMapMemory(device.allocator, vertexBuffer.allocation, &data);
    memcpy(data, mesh.vertices.data(), vertexBufferSize);
    vmaUnmapMemory(device.allocator, vertexBuffer.allocation);
    
    // Create index buffer
    VkDeviceSize indexBufferSize = sizeof(uint32_t) * mesh.indices.size();
    
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = indexBufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    
    VmaAllocationInfo allocInfoResult;
    if (vmaCreateBuffer(device.allocator, &bufferInfo, &allocInfo, &indexBuffer.buffer, 
                       &indexBuffer.allocation, &allocInfoResult) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create index buffer!");
    }
    
    // Upload index data
    memcpy(allocInfoResult.pMappedData, mesh.indices.data(), indexBufferSize);
}

