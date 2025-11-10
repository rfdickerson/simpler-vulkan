#pragma once

#include <functional>
#include <vector>
#include <glm/glm.hpp>
#include "hex_coord.hpp"
#include "terrain.hpp"

// Vertex structure for terrain rendering
struct TerrainVertex {
    glm::vec3 position;    // World space position
    glm::vec3 normal;      // Normal (for lighting)
    glm::vec2 uv;          // UV coordinates (for texturing)
    glm::vec2 hexCoord;    // Hex coordinates (q, r) for procedural effects
    uint32_t terrainType;  // Terrain type ID
    
    TerrainVertex()
        : position(0.0f)
        , normal(0.0f, 1.0f, 0.0f)
        , uv(0.0f)
        , hexCoord(0.0f)
        , terrainType(0)
    {}
    
    TerrainVertex(glm::vec3 pos, glm::vec3 norm, glm::vec2 uv_, glm::vec2 hexC, uint32_t type = 0)
        : position(pos)
        , normal(norm)
        , uv(uv_)
        , hexCoord(hexC)
        , terrainType(type)
    {}
};

// Hex mesh generator
class HexMesh {
public:
    std::vector<TerrainVertex> vertices;
    std::vector<uint32_t> indices;
    
    // Generate a single hex tile mesh (6 triangles forming a hexagon)
    static HexMesh generateSingleHex(const HexCoord& hex, float hexSize, float height = 0.0f, uint32_t terrainType = 0) {
        HexMesh mesh;
        
        glm::vec3 center = hexToWorld(hex, hexSize);
        center.y = height;
        
        // Center vertex
        mesh.vertices.push_back(TerrainVertex(
            center,
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec2(0.5f, 0.5f),
            glm::vec2(hex.q, hex.r),
            terrainType
        ));
        
        // 6 outer vertices (flat-top orientation: vertices at right, top-right, top-left, left, bottom-left, bottom-right)
        for (int i = 0; i < 6; ++i) {
            float angle_deg = 60.0f * i;  // Start at 0° (right) for flat-top
            float angle_rad = glm::radians(angle_deg);
            
            glm::vec3 pos = center + glm::vec3(
                hexSize * std::cos(angle_rad),
                0.0f,
                hexSize * std::sin(angle_rad)
            );
            pos.y = height;
            
            // UV: map to unit circle, then to [0, 1]
            float u = 0.5f + 0.5f * std::cos(angle_rad);
            float v = 0.5f + 0.5f * std::sin(angle_rad);
            
            mesh.vertices.push_back(TerrainVertex(
                pos,
                glm::vec3(0.0f, 1.0f, 0.0f),
                glm::vec2(u, v),
                glm::vec2(hex.q, hex.r),
                terrainType
            ));
        }
        
        // Create triangles (center to each edge)
        for (int i = 0; i < 6; ++i) {
            mesh.indices.push_back(0);              // Center
            mesh.indices.push_back(1 + i);          // Current vertex
            mesh.indices.push_back(1 + (i + 1) % 6); // Next vertex
        }
        
        return mesh;
    }
    
    // Generate mesh for multiple hexes
    static HexMesh generateHexGrid(const std::vector<HexCoord>& hexes, float hexSize, 
                                   const std::function<float(const HexCoord&)>& heightFunc = nullptr,
                                   const std::function<uint32_t(const HexCoord&)>& typeFunc = nullptr) {
        HexMesh mesh;
        
        for (const auto& hex : hexes) {
            float height = heightFunc ? heightFunc(hex) : 0.0f;
            uint32_t terrainType = typeFunc ? typeFunc(hex) : 0;
            HexMesh singleHex = generateSingleHex(hex, hexSize, height, terrainType);
            
            // Offset indices for the new hex
            uint32_t vertexOffset = static_cast<uint32_t>(mesh.vertices.size());
            
            // Add vertices
            mesh.vertices.insert(mesh.vertices.end(), 
                               singleHex.vertices.begin(), 
                               singleHex.vertices.end());
            
            // Add indices with offset
            for (uint32_t idx : singleHex.indices) {
                mesh.indices.push_back(idx + vertexOffset);
            }
        }
        
        return mesh;
    }
    
    // Generate a rectangular hex grid
    static HexMesh generateRectangularGrid(int width, int height, float hexSize) {
        std::vector<HexCoord> hexes;
        
        // Generate hexes in a rectangular pattern (pointy-top)
        for (int r = 0; r < height; ++r) {
            int rOffset = r / 2; // Offset for odd rows
            for (int q = -rOffset; q < width - rOffset; ++q) {
                hexes.push_back(HexCoord(q, r));
            }
        }
        
        return generateHexGrid(hexes, hexSize);
    }
    
    // Generate a radial hex grid around a center point
    static HexMesh generateRadialGrid(const HexCoord& center, int radius, float hexSize) {
        std::vector<HexCoord> hexes = hexesInRadius(center, radius);
        return generateHexGrid(hexes, hexSize);
    }
    
    // Subdivide a hex for smoother curvature (useful for tessellation fallback)
    static HexMesh generateSubdividedHex(const HexCoord& hex, float hexSize, int subdivisions) {
        HexMesh mesh;
        
        glm::vec3 center = hexToWorld(hex, hexSize);
        
        // For now, just return a regular hex (tessellation will handle subdivision)
        // This can be expanded later for non-tessellation fallback
        return generateSingleHex(hex, hexSize);
    }
    
    // Merge another mesh into this one
    void merge(const HexMesh& other) {
        uint32_t vertexOffset = static_cast<uint32_t>(vertices.size());
        
        vertices.insert(vertices.end(), other.vertices.begin(), other.vertices.end());
        
        for (uint32_t idx : other.indices) {
            indices.push_back(idx + vertexOffset);
        }
    }
    
    // Recalculate normals (for smooth shading)
    void recalculateNormals() {
        // Reset all normals
        for (auto& vertex : vertices) {
            vertex.normal = glm::vec3(0.0f);
        }
        
        // Accumulate face normals
        for (size_t i = 0; i < indices.size(); i += 3) {
            uint32_t i0 = indices[i];
            uint32_t i1 = indices[i + 1];
            uint32_t i2 = indices[i + 2];
            
            glm::vec3 v0 = vertices[i0].position;
            glm::vec3 v1 = vertices[i1].position;
            glm::vec3 v2 = vertices[i2].position;
            
            glm::vec3 edge1 = v1 - v0;
            glm::vec3 edge2 = v2 - v0;
            glm::vec3 faceNormal = glm::normalize(glm::cross(edge1, edge2));
            
            vertices[i0].normal += faceNormal;
            vertices[i1].normal += faceNormal;
            vertices[i2].normal += faceNormal;
        }
        
        // Normalize all normals
        for (auto& vertex : vertices) {
            vertex.normal = glm::normalize(vertex.normal);
        }
    }
    
    // Get vertex input binding description for Vulkan
    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(TerrainVertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }
    
    // Get vertex attribute descriptions for Vulkan
    static std::array<VkVertexInputAttributeDescription, 5> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 5> attributeDescriptions{};
        
        // Position
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(TerrainVertex, position);
        
        // Normal
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(TerrainVertex, normal);
        
        // UV
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(TerrainVertex, uv);
        
        // Hex Coord
        attributeDescriptions[3].binding = 0;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[3].offset = offsetof(TerrainVertex, hexCoord);
        
        // Terrain Type
        attributeDescriptions[4].binding = 0;
        attributeDescriptions[4].location = 4;
        attributeDescriptions[4].format = VK_FORMAT_R32_UINT;
        attributeDescriptions[4].offset = offsetof(TerrainVertex, terrainType);
        
        return attributeDescriptions;
    }
};

