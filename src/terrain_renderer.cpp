#include "terrain_renderer.hpp"
#include <iostream>

TerrainRenderer::TerrainRenderer(Device& device, float hexSize)
    : device(device)
    , hexSize(hexSize)
    , meshDirty(true)
    , vertexBuffer{VK_NULL_HANDLE, nullptr}
    , indexBuffer{VK_NULL_HANDLE, nullptr}
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
    
    // Flat-top "odd-q" offset rectangle mapped to axial coordinates
    // offset (col, row) -> axial: q = col, r = row - floor(col/2)
    for (int col = 0; col < width; ++col) {
        int q = col;
        int qOffset = col / 2; // floor for ints
        for (int row = 0; row < height; ++row) {
            int r = row - qOffset;
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

void TerrainRenderer::initializeSimpleBiomeMap(int width, int height) {
    tiles.clear();
    tileOrder.clear();

    // Build flat-top "odd-q" rectangle and assign simple biomes by row bands:
    // top band = Ocean, middle = Grassland, bottom = Desert
    for (int col = 0; col < width; ++col) {
        int q = col;
        int qOffset = col / 2;
        for (int row = 0; row < height; ++row) {
            int r = row - qOffset;
            HexCoord hex(q, r);
            tileOrder.push_back(hex);

            float t = (height > 1) ? (static_cast<float>(row) / static_cast<float>(height - 1)) : 0.0f;

            TerrainTile tile;
            if (t < 0.33f) {
                tile.type = TerrainType::Ocean;
            } else if (t < 0.66f) {
                tile.type = TerrainType::Grassland;
            } else {
                tile.type = TerrainType::Desert;
            }

            tile.height = 0.0f;
            tile.explored = 255;
            tile.visible = 255;
            tiles[hex] = tile;
        }
    }

    meshDirty = true;
    std::cout << "Initialized simple biome map: " << width << "x" << height
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
        },
        [this](const HexCoord& hex) -> uint32_t {
            auto it = tiles.find(hex);
            return (it != tiles.end()) ? static_cast<uint32_t>(it->second.type) : 0;
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
        throw std::runtime_error("Failed to create vertex buffer!");
    }
    
    // Upload vertex data
    memcpy(vertexAllocInfoResult.pMappedData, mesh.vertices.data(), vertexBufferSize);
    
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

