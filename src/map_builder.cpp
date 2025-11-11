#include "map_builder.hpp"
#include "noise.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>

void MapBuilder::generateMap(TerrainRenderer& renderer, const MapConfig& config) {
    std::cout << "Generating map with seed " << config.seed 
              << " (" << config.width << "x" << config.height << ")..." << std::endl;
    
    // Step 0: Initialize empty grid structure
    renderer.initializeEmptyGrid(config.width, config.height);
    
    // Step 1: Generate elevation (and moisture if enabled)
    ElevationData data = generateElevationMap(config);
    normalizeElevation(data);
    
    // Step 2: Assign biomes based on elevation and moisture
    assignBiomes(renderer, data, config);
    
    // Step 3: Add coastal water for visual variety
    addCoastalWater(renderer, config);
    
    // Step 4: Rebuild the mesh
    renderer.rebuildMesh();
    
    std::cout << "Map generation complete!" << std::endl;
}

MapBuilder::ElevationData MapBuilder::generateElevationMap(const MapConfig& config) {
    ElevationData data;
    SimplexNoise elevationNoise(config.seed);
    SimplexNoise moistureNoise(config.seed + 1000); // Different seed for moisture
    
    data.elevation.resize(config.height, std::vector<float>(config.width));
    if (config.useMoistureMap) {
        data.moisture.resize(config.height, std::vector<float>(config.width));
    }
    
    data.minElevation = 1.0f;
    data.maxElevation = 0.0f;
    
    // Generate elevation using fractal noise
    for (int row = 0; row < config.height; ++row) {
        for (int col = 0; col < config.width; ++col) {
            float x = col * config.frequency;
            float y = row * config.frequency;
            
            // Generate elevation with fractal noise
            float elevation = elevationNoise.fractalNoise(x, y, config.octaves, 
                                                          config.persistence, config.lacunarity);
            
            // Add some island shape bias (fade edges slightly)
            float centerX = config.width * 0.5f;
            float centerY = config.height * 0.5f;
            float distX = (col - centerX) / centerX;
            float distY = (row - centerY) / centerY;
            float distFromCenter = std::sqrt(distX * distX + distY * distY);
            float islandBias = 1.0f - std::min(1.0f, distFromCenter * 0.5f);
            
            elevation = elevation * 0.7f + islandBias * 0.3f;
            
            data.elevation[row][col] = elevation;
            data.minElevation = std::min(data.minElevation, elevation);
            data.maxElevation = std::max(data.maxElevation, elevation);
            
            // Generate moisture map if enabled
            if (config.useMoistureMap) {
                float mx = col * config.moistureFrequency;
                float my = row * config.moistureFrequency;
                data.moisture[row][col] = moistureNoise.fractalNoise(mx, my, 3, 0.5f);
            }
        }
    }
    
    return data;
}

void MapBuilder::normalizeElevation(ElevationData& data) {
    float range = data.maxElevation - data.minElevation;
    if (range < 0.001f) range = 1.0f; // Avoid division by zero
    
    for (auto& row : data.elevation) {
        for (auto& val : row) {
            val = (val - data.minElevation) / range;
        }
    }
}

TerrainType MapBuilder::getBiomeFromElevation(float elevation, float moisture, 
                                               float latitude, const MapConfig& config) {
    // Water biomes
    if (elevation < config.waterLevel) {
        return TerrainType::Ocean;
    }
    
    // Latitude goes from 0 (top) to 1 (bottom)
    // Top and bottom = cold, middle = temperate/warm
    float distFromEquator = std::abs(latitude - 0.5f) * 2.0f; // 0 at equator, 1 at poles
    
    // Very cold regions (poles)
    if (distFromEquator > 0.75f) {
        if (elevation < config.hillLevel) {
            return TerrainType::Tundra;
        } else if (elevation < config.mountainLevel) {
            return TerrainType::Hills; // Rocky hills in tundra
        } else {
            return TerrainType::Ice; // Ice peaks
        }
    }
    
    // Cold regions
    if (distFromEquator > 0.6f) {
        if (elevation < config.hillLevel) {
            return moisture > 0.5f ? TerrainType::Tundra : TerrainType::Plains;
        } else if (elevation < config.mountainLevel) {
            return TerrainType::Hills;
        } else {
            return TerrainType::Mountains;
        }
    }
    
    // Temperate and tropical regions
    if (elevation >= config.mountainLevel) {
        return TerrainType::Mountains;
    } else if (elevation >= config.hillLevel) {
        return TerrainType::Hills;
    }
    
    // Low elevation - use moisture for biome variety
    // Near equator (latitude ~ 0.5), more tropical biomes
    bool isTropical = distFromEquator < 0.3f;
    
    if (moisture < 0.3f) {
        // Dry
        if (isTropical || distFromEquator < 0.4f) {
            return TerrainType::Desert;
        } else {
            return TerrainType::Plains;
        }
    } else if (moisture < 0.5f) {
        // Moderate moisture
        return TerrainType::Plains;
    } else if (moisture < 0.7f) {
        // Moist
        return TerrainType::Grassland;
    } else {
        // Very moist
        if (isTropical) {
            return TerrainType::Jungle;
        } else {
            return TerrainType::Forest;
        }
    }
}

void MapBuilder::assignBiomes(TerrainRenderer& renderer, const ElevationData& data, 
                               const MapConfig& config) {
    // Build flat-top "odd-q" rectangle and assign biomes
    for (int col = 0; col < config.width; ++col) {
        int q = col;
        int qOffset = col / 2;
        for (int row = 0; row < config.height; ++row) {
            int r = row - qOffset;
            HexCoord hex(q, r);
            
            float elevation = data.elevation[row][col];
            float moisture = config.useMoistureMap ? data.moisture[row][col] : 0.5f;
            float latitude = static_cast<float>(row) / static_cast<float>(config.height - 1);
            
            TerrainType type = getBiomeFromElevation(elevation, moisture, latitude, config);
            
            // Set terrain tile
            renderer.setTerrainType(hex, type);
            
            // Set height based on elevation (scaled appropriately)
            float height = 0.0f;
            if (elevation >= config.waterLevel) {
                // Land has varying height
                height = (elevation - config.waterLevel) / (1.0f - config.waterLevel) * 0.5f;
            }
            renderer.setTerrainHeight(hex, height);
        }
    }
}

void MapBuilder::addCoastalWater(TerrainRenderer& renderer, const MapConfig& config) {
    const auto& tiles = renderer.getTiles();
    std::vector<HexCoord> coastalHexes;
    
    // Find all ocean tiles adjacent to land
    for (const auto& [hex, tile] : tiles) {
        if (tile.type == TerrainType::Ocean) {
            // Check neighbors
            bool adjacentToLand = false;
            for (int dir = 0; dir < 6; ++dir) {
                HexCoord neighbor = hexNeighbor(hex, dir);
                auto it = tiles.find(neighbor);
                if (it != tiles.end() && it->second.type != TerrainType::Ocean 
                    && it->second.type != TerrainType::CoastalWater) {
                    adjacentToLand = true;
                    break;
                }
            }
            
            if (adjacentToLand) {
                coastalHexes.push_back(hex);
            }
        }
    }
    
    // Update coastal hexes to coastal water
    for (const auto& hex : coastalHexes) {
        renderer.setTerrainType(hex, TerrainType::CoastalWater);
    }
    
    std::cout << "Added " << coastalHexes.size() << " coastal water tiles" << std::endl;
}

