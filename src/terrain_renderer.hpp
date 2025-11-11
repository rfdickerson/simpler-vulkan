#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include "device.hpp"
#include "buffer.hpp"
#include "camera.hpp"
#include "hex_coord.hpp"
#include "hex_mesh.hpp"
#include "terrain.hpp"
#include "terrain_pipeline.hpp"

// Terrain renderer - manages all hex tiles and rendering
class TerrainRenderer {
public:
    TerrainRenderer(Device& device, float hexSize = 1.0f);
    ~TerrainRenderer();
    
    // Initialize terrain with a specific pattern
    void initializeRectangularGrid(int width, int height);
    // Flat-top hex "odd-q" rectangle with simple biomes (water/grass/desert)
    void initializeSimpleBiomeMap(int width, int height);
    void initializeRadialGrid(const HexCoord& center, int radius);
    // Initialize empty grid for external map builders
    void initializeEmptyGrid(int width, int height);
    
    // Set terrain type for a specific hex
    void setTerrainType(const HexCoord& hex, TerrainType type);
    void setTerrainHeight(const HexCoord& hex, float height);
    
    // Update terrain mesh (call after modifying terrain)
    void rebuildMesh();
    
    // Update rendering parameters
    void updateRenderParams(const Camera& camera, float time);
    
    // Get terrain parameters for modification
    TerrainRenderParams& getRenderParams() { return renderParams; }
    const TerrainRenderParams& getRenderParams() const { return renderParams; }
    
    // Get mesh data
    const HexMesh& getMesh() const { return mesh; }
    const Buffer& getVertexBuffer() const { return vertexBuffer; }
    const Buffer& getIndexBuffer() const { return indexBuffer; }
    uint32_t getIndexCount() const { return static_cast<uint32_t>(mesh.indices.size()); }
    
    // Get terrain data
    const std::unordered_map<HexCoord, TerrainTile>& getTiles() const { return tiles; }
    
private:
    Device& device;
    float hexSize;
    
    // Terrain data
    std::unordered_map<HexCoord, TerrainTile> tiles;
    std::vector<HexCoord> tileOrder; // For consistent ordering
    
    // Mesh and buffers
    HexMesh mesh;
    Buffer vertexBuffer;
    Buffer indexBuffer;
    bool meshDirty;
    
    // Rendering parameters
    TerrainRenderParams renderParams;
    
    // Upload mesh to GPU
    void uploadMeshToGPU();
};

