#pragma once

#include <vector>
#include <cstdint>
#include "terrain_renderer.hpp"
#include "hex_coord.hpp"
#include "terrain.hpp"

// Configuration for map generation
struct MapConfig {
    int width = 40;                 // Map width in hexes
    int height = 24;                // Map height in hexes
    uint32_t seed = 12345;          // Random seed for reproducibility
    float waterLevel = 0.4f;        // 0-1, percentage of map that's water
    float mountainLevel = 0.7f;     // Threshold for mountains
    float hillLevel = 0.55f;        // Threshold for hills
    int octaves = 4;                // Noise detail level (more = more detail)
    float frequency = 0.08f;        // Continent size (lower = larger continents)
    float persistence = 0.5f;       // How much each octave contributes
    float lacunarity = 2.0f;        // Frequency multiplier between octaves
    bool useMoistureMap = true;     // Whether to use moisture for desert/forest placement
    float moistureFrequency = 0.12f; // Frequency for moisture noise
};

// MapBuilder - generates realistic terrain maps using noise
class MapBuilder {
public:
    // Generate a complete map and populate the terrain renderer
    static void generateMap(TerrainRenderer& renderer, const MapConfig& config = MapConfig());
    
private:
    // Internal generation steps
    struct ElevationData {
        std::vector<std::vector<float>> elevation;
        std::vector<std::vector<float>> moisture;
        float minElevation;
        float maxElevation;
    };
    
    static ElevationData generateElevationMap(const MapConfig& config);
    static void normalizeElevation(ElevationData& data);
    static TerrainType getBiomeFromElevation(float elevation, float moisture, float latitude, const MapConfig& config);
    static void assignBiomes(TerrainRenderer& renderer, const ElevationData& data, const MapConfig& config);
    static void addCoastalWater(TerrainRenderer& renderer, const MapConfig& config);
};


