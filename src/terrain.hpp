#pragma once

#include <cstdint>
#include <string>
#include <glm/glm.hpp>

// Terrain types matching the art-style document
enum class TerrainType : uint8_t {
    Ocean = 0,
    CoastalWater,
    Grassland,
    Plains,
    Forest,
    Jungle,
    Hills,
    Mountains,
    Desert,
    Dunes,
    Swamp,
    Marsh,
    Tundra,
    Ice,
    River,
    NaturalWonder,
    
    Count // Keep at end
};

// Terrain properties for rendering and gameplay
struct TerrainProperties {
    const char* name;
    glm::vec3 baseColor;      // Base color for the terrain
    float roughness;          // PBR roughness
    float metallic;           // PBR metallic
    bool animated;            // Whether terrain has animated effects
    bool translucent;         // Whether terrain is translucent (water)
    float movementCost;       // Gameplay: movement cost multiplier
    
    static TerrainProperties get(TerrainType type) {
        switch (type) {
            case TerrainType::Ocean:
                return {"Ocean", glm::vec3(0.1f, 0.3f, 0.5f), 0.1f, 0.0f, true, true, 2.0f};
            case TerrainType::CoastalWater:
                return {"Coastal Water", glm::vec3(0.2f, 0.5f, 0.6f), 0.1f, 0.0f, true, true, 1.5f};
            case TerrainType::Grassland:
                return {"Grassland", glm::vec3(0.4f, 0.6f, 0.2f), 0.8f, 0.0f, false, false, 1.0f};
            case TerrainType::Plains:
                return {"Plains", glm::vec3(0.7f, 0.6f, 0.3f), 0.7f, 0.0f, false, false, 1.0f};
            case TerrainType::Forest:
                return {"Forest", glm::vec3(0.2f, 0.4f, 0.1f), 0.9f, 0.0f, true, false, 2.0f};
            case TerrainType::Jungle:
                return {"Jungle", glm::vec3(0.15f, 0.35f, 0.15f), 0.9f, 0.0f, true, false, 2.5f};
            case TerrainType::Hills:
                return {"Hills", glm::vec3(0.5f, 0.5f, 0.3f), 0.8f, 0.0f, false, false, 2.0f};
            case TerrainType::Mountains:
                return {"Mountains", glm::vec3(0.4f, 0.4f, 0.4f), 0.7f, 0.0f, false, false, 3.0f};
            case TerrainType::Desert:
                return {"Desert", glm::vec3(0.9f, 0.8f, 0.5f), 0.9f, 0.0f, true, false, 2.0f};
            case TerrainType::Dunes:
                return {"Dunes", glm::vec3(0.95f, 0.85f, 0.6f), 0.9f, 0.0f, true, false, 2.0f};
            case TerrainType::Swamp:
                return {"Swamp", glm::vec3(0.3f, 0.3f, 0.2f), 0.6f, 0.0f, true, false, 3.0f};
            case TerrainType::Marsh:
                return {"Marsh", glm::vec3(0.4f, 0.4f, 0.3f), 0.7f, 0.0f, true, false, 2.5f};
            case TerrainType::Tundra:
                return {"Tundra", glm::vec3(0.8f, 0.85f, 0.9f), 0.5f, 0.0f, false, false, 2.0f};
            case TerrainType::Ice:
                return {"Ice", glm::vec3(0.9f, 0.95f, 1.0f), 0.1f, 0.2f, false, false, 3.0f};
            case TerrainType::River:
                return {"River", glm::vec3(0.3f, 0.5f, 0.7f), 0.1f, 0.0f, true, true, 1.5f};
            case TerrainType::NaturalWonder:
                return {"Natural Wonder", glm::vec3(0.8f, 0.6f, 1.0f), 0.3f, 0.5f, true, false, 1.0f};
            default:
                return {"Unknown", glm::vec3(1.0f, 0.0f, 1.0f), 0.5f, 0.0f, false, false, 1.0f};
        }
    }
};

// Individual tile data
struct TerrainTile {
    TerrainType type;
    float height;              // Elevation for displacement
    uint8_t explored;          // Fog of war state (0 = unexplored, 255 = fully explored)
    uint8_t visible;           // Current visibility (0 = not visible, 255 = visible)
    uint16_t features;         // Bitfield for additional features (forests, rivers, etc.)
    
    TerrainTile() 
        : type(TerrainType::Grassland)
        , height(0.0f)
        , explored(0)
        , visible(0)
        , features(0)
    {}
};

// Era visual style (affects LUT and lighting)
enum class Era : uint8_t {
    Discovery = 0,      // Warm, saturated, optimistic
    Enlightenment,      // Neutral whites, cool marble tones
    Industrial,         // Desaturated, foggy, industrial
    
    Count
};

// Global rendering parameters
struct TerrainRenderParams {
    float hexSize;             // Size of each hex tile
    float time;                // Elapsed time for animations
    Era currentEra;            // Current visual era
    glm::vec3 sunDirection;    // Direction to sun (for lighting)
    glm::vec3 sunColor;        // Sun color
    float ambientIntensity;    // Ambient light intensity
    
    TerrainRenderParams()
        : hexSize(1.0f)
        , time(0.0f)
        , currentEra(Era::Discovery)
        , sunDirection(glm::normalize(glm::vec3(0.3f, -0.5f, 0.4f)))
        , sunColor(1.0f, 0.95f, 0.8f)
        , ambientIntensity(0.3f)
    {}
};

