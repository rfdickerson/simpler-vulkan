# Hex Tile Renderer Foundation

**Created:** November 2025  
**Status:** Foundation Complete - Ready for Development

---

## 📋 Overview

This document describes the foundational systems for the hex-based terrain renderer as specified in `art-style.md`. The foundation provides everything needed to start building the "Age of Exploration" diorama-style strategy game.

---

## 🏗️ Core Components

### 1. Hex Coordinate System (`hex_coord.hpp`)

**Features:**
- Axial coordinate system (q, r) for hex tiles
- Flat-top hex orientation
- Neighbor finding and distance calculations
- World-space conversion functions
- Radius-based hex generation

**Key Functions:**
```cpp
HexCoord hex(5, 3);                                 // Create hex at (5, 3)
glm::vec3 worldPos = hexToWorld(hex, hexSize);     // Convert to world position
std::vector<HexCoord> neighbors = hexNeighbors(hex); // Get all 6 neighbors
int distance = hexDistance(hex1, hex2);             // Manhattan distance in hex space
std::vector<HexCoord> area = hexesInRadius(center, 10); // Get all hexes in radius
```

**Technical Details:**
- Uses standard axial coordinates (reference: Red Blob Games)
- Supports use in `std::unordered_map` via custom hash function
- All conversions are inline for performance

---

### 2. Terrain System (`terrain.hpp`)

**Terrain Types:**
Includes all 16+ terrain types from the art-style document:
- **Water:** Ocean, Coastal Water, Rivers
- **Land:** Grassland, Plains, Hills, Mountains
- **Biomes:** Forest, Jungle, Desert, Dunes, Swamp, Tundra, Ice
- **Special:** Natural Wonders

**Data Structures:**

```cpp
// Terrain properties for rendering
struct TerrainProperties {
    const char* name;
    glm::vec3 baseColor;     // Base color
    float roughness;         // PBR roughness
    float metallic;          // PBR metallic
    bool animated;           // Whether terrain animates
    bool translucent;        // For water/glass effects
    float movementCost;      // Gameplay parameter
};

// Per-tile data
struct TerrainTile {
    TerrainType type;
    float height;            // Elevation for displacement
    uint8_t explored;        // Fog of war (0-255)
    uint8_t visible;         // Current visibility
    uint16_t features;       // Bitfield for features
};

// Rendering parameters
struct TerrainRenderParams {
    float hexSize;
    float time;              // For animations
    Era currentEra;          // Discovery/Enlightenment/Industrial
    glm::vec3 sunDirection;
    glm::vec3 sunColor;
    float ambientIntensity;
};
```

**Era System:**
- **Discovery:** Warm, saturated, optimistic colors
- **Enlightenment:** Neutral whites, cool marble tones
- **Industrial:** Desaturated, foggy, industrial atmosphere

---

### 3. Hex Mesh Generator (`hex_mesh.hpp`)

**Features:**
- Procedural hex mesh generation
- Rectangular and radial grid patterns
- Per-vertex height support
- Efficient mesh merging
- Vulkan vertex format integration

**Vertex Structure:**
```cpp
struct TerrainVertex {
    glm::vec3 position;    // World space position
    glm::vec3 normal;      // Lighting normal
    glm::vec2 uv;          // Texture coordinates
    glm::vec2 hexCoord;    // Hex (q,r) for procedural effects
};
```

**Usage Examples:**
```cpp
// Single hex
HexMesh singleHex = HexMesh::generateSingleHex(HexCoord(0, 0), hexSize, height);

// Rectangular grid
HexMesh grid = HexMesh::generateRectangularGrid(20, 15, hexSize);

// Radial grid (for focused gameplay)
HexMesh radial = HexMesh::generateRadialGrid(HexCoord(0, 0), 10, hexSize);

// Custom grid with height function
std::vector<HexCoord> hexes = {...};
HexMesh custom = HexMesh::generateHexGrid(hexes, hexSize, 
    [](const HexCoord& hex) { return perlinNoise(hex.q, hex.r); });
```

---

### 4. Camera System (`camera.hpp`)

**Diorama-Style Camera:**
- Orbit around target point
- Tilt angle control (looking down at miniatures)
- Pan, zoom, and rotate controls
- Perspective projection

**Key Parameters:**
```cpp
Camera camera;
camera.tiltAngle = 60.0f;      // Look down at 60° (tilt-shift style)
camera.orbitRadius = 15.0f;    // Distance from target
camera.orbitAngle = 45.0f;     // Rotation around Y axis
camera.fov = 45.0f;            // Field of view

// Controls
camera.pan(dx, dz);            // Move camera target
camera.zoom(delta);            // Adjust orbit radius
camera.rotate(angleDelta);     // Rotate around target
camera.tilt(angleDelta);       // Adjust view angle
```

**View Matrices:**
```cpp
glm::mat4 view = camera.getViewMatrix();
glm::mat4 proj = camera.getProjectionMatrix();
glm::mat4 viewProj = camera.getViewProjectionMatrix();
```

---

### 5. Terrain Shaders

#### Vertex Shader (`terrain.vert`)
**Inputs:**
- Vertex position, normal, UV, hex coordinates
- View-projection matrix via push constants

**Outputs:**
- Transformed position (clip space)
- World position, normal, UV, hex coord (for fragment shader)

#### Fragment Shader (`terrain.frag`)
**Features:**
- Procedural terrain coloring based on hex coordinates
- Simple PBR-inspired lighting (diffuse + specular + ambient)
- Hex edge detection for border highlights
- Era-based color grading
- Noise-based terrain variation

**Lighting:**
- Directional sun light
- Ambient lighting
- Fake AO from surface curvature
- Specular highlights

**Visual Effects:**
- Parchment-like edge highlights (warm glow)
- Procedural color variation per hex
- Era-based post-processing (desaturation, color shifts)

#### Compute Shader (`terrain_cull.comp`)
**Purpose:** Frustum culling for performance optimization

**Features:**
- GPU-based frustum culling
- Sphere-vs-frustum tests
- Atomic counter for visible tile count
- Outputs list of visible tile indices

**Usage:** (Future implementation)
```cpp
// Dispatch compute shader to cull tiles
// Use visible indices for indirect drawing
```

---

### 6. Terrain Pipeline (`terrain_pipeline.hpp/cpp`)

**Vulkan Pipeline:**
- Dynamic rendering (Vulkan 1.3)
- Push constants for per-frame data
- Uniform buffer for terrain parameters
- Descriptor set for uniform buffer
- Back-face culling enabled
- Opaque rendering (translucent water can be added later)

**Push Constants:**
```cpp
struct TerrainPushConstants {
    glm::mat4 viewProj;    // View-projection matrix
    glm::vec3 cameraPos;   // Camera position (for specular)
    float time;            // Elapsed time (for animations)
};
```

**Uniform Buffer:**
```cpp
struct TerrainParamsUBO {
    glm::vec3 sunDirection;
    glm::vec3 sunColor;
    float ambientIntensity;
    float hexSize;
    int32_t currentEra;
};
```

---

### 7. Terrain Renderer (`terrain_renderer.hpp/cpp`)

**High-Level Manager:**
Manages all terrain tiles, mesh generation, and GPU resources.

**Key Methods:**
```cpp
TerrainRenderer terrain(device, hexSize);

// Initialize terrain
terrain.initializeRectangularGrid(30, 20);  // 30x20 grid
terrain.initializeRadialGrid(center, 15);   // Radial with radius 15

// Modify terrain
terrain.setTerrainType(hex, TerrainType::Mountains);
terrain.setTerrainHeight(hex, 2.0f);

// Rebuild mesh (call after modifications)
terrain.rebuildMesh();

// Update rendering (per frame)
terrain.updateRenderParams(camera, elapsedTime);

// Access data
const HexMesh& mesh = terrain.getMesh();
const Buffer& vertexBuffer = terrain.getVertexBuffer();
uint32_t indexCount = terrain.getIndexCount();
```

**Resource Management:**
- Automatic GPU buffer creation/destruction
- Efficient mesh rebuilding (only when dirty)
- VMA-based memory allocation

---

### 8. Example Usage (`terrain_example.hpp`)

**Complete Working Example:**
```cpp
// Create terrain example
TerrainExample example(device, swapchain);

// Update loop
void update(float deltaTime) {
    example.update(deltaTime);
    
    // Optional: camera controls
    Camera& camera = example.getCamera();
    camera.rotate(deltaTime * 10.0f);  // Rotate camera
}

// Render loop
void render(VkCommandBuffer cmd) {
    vkCmdBeginRendering(cmd, &renderingInfo);
    example.render(cmd);
    vkCmdEndRendering(cmd);
}
```

---

## 🎯 Implementation Status

### ✅ Completed Foundation
- [x] Hex coordinate system with all utility functions
- [x] Terrain type system with 16+ terrain types
- [x] Terrain tile data structures
- [x] Era-based rendering parameters
- [x] Hex mesh generator (single, rectangular, radial patterns)
- [x] 3D camera system with diorama-style controls
- [x] Terrain vertex and fragment shaders
- [x] Frustum culling compute shader
- [x] Terrain rendering pipeline (Vulkan)
- [x] Terrain renderer manager class
- [x] Working example/demo setup
- [x] CMake integration
- [x] GLM dependency added

### 🚧 Next Steps for Full Renderer

1. **Tessellation Shaders**
   - Add tessellation control/evaluation shaders
   - Implement smooth curvature for miniature effect
   - LOD-based tessellation

2. **Texture System**
   - Triplanar texture blending
   - Terrain type texture atlas
   - Normal maps for detail

3. **Water Rendering**
   - Animated water shader
   - Refractive/reflective surfaces
   - Foam at coastlines

4. **Post-Processing**
   - Tilt-shift depth of field
   - Bloom for resin-like highlights
   - LUT-based color grading per era
   - Vignette effect

5. **Fog of War**
   - Parchment map shader for unexplored areas
   - Animated reveal effect
   - Decorative compass roses

6. **Advanced Features**
   - Rivers and coastlines (flow maps)
   - Forests (billboard cards with sway)
   - Mountains (height-based snow masks)
   - Natural wonders (custom FX)

---

## 🔧 Technical Architecture

### Rendering Flow
```
1. TerrainRenderer manages tile data
2. When tiles change → rebuild mesh
3. Per frame:
   - Update camera matrices
   - Update terrain uniform buffer
   - Bind terrain pipeline
   - Bind descriptor sets
   - Push constants (viewProj, camera, time)
   - Bind vertex/index buffers
   - Draw indexed
```

### Memory Layout
```
CPU Side:
  - TerrainTile map (std::unordered_map<HexCoord, TerrainTile>)
  - HexMesh (vertices + indices vectors)
  
GPU Side:
  - Vertex buffer (VMA_MEMORY_USAGE_CPU_TO_GPU)
  - Index buffer (VMA_MEMORY_USAGE_CPU_TO_GPU)
  - Uniform buffer (VMA_MEMORY_USAGE_CPU_TO_GPU, persistently mapped)
```

### Performance Considerations
- **Mesh Generation:** CPU-based, only when terrain changes
- **Culling:** Can use compute shader for large maps (future)
- **Draw Calls:** Single draw call per frame (can be indirect in future)
- **Memory:** Efficient packing (HexCoord uses 8 bytes)

---

## 📚 File Structure

```
src/
  ├── hex_coord.hpp          # Hex coordinate system
  ├── terrain.hpp            # Terrain types and data
  ├── hex_mesh.hpp           # Mesh generation
  ├── camera.hpp             # 3D camera
  ├── terrain_pipeline.hpp   # Vulkan pipeline
  ├── terrain_pipeline.cpp
  ├── terrain_renderer.hpp   # High-level manager
  ├── terrain_renderer.cpp
  └── terrain_example.hpp    # Usage example

shaders/
  ├── terrain.vert           # Vertex shader
  ├── terrain.frag           # Fragment shader
  ├── terrain_cull.comp      # Culling compute shader
  └── *.spv                  # Compiled SPIR-V

design/
  ├── art-style.md           # Art direction document
  └── HEX_RENDERER_FOUNDATION.md  # This file
```

---

## 🎮 Integration with Main Application

To integrate with `main.cpp`:

1. **Replace triangle rendering with terrain:**
```cpp
#include "terrain_example.hpp"

// In main()
TerrainExample terrainExample(device, swapchain);

// In render loop
terrainExample.update(deltaTime);
terrainExample.render(cmd);
```

2. **Add camera controls:**
```cpp
Camera& camera = terrainExample.getCamera();

// Handle input
if (keyPressed(KEY_W)) camera.pan(0, -speed * deltaTime);
if (keyPressed(KEY_S)) camera.pan(0, speed * deltaTime);
if (keyPressed(KEY_A)) camera.pan(-speed * deltaTime, 0);
if (keyPressed(KEY_D)) camera.pan(speed * deltaTime, 0);
if (scrollWheel) camera.zoom(scrollDelta);
```

---

## 🌟 Key Design Principles

Following the art-style document mantra:
> **"If it moves or blends, shader it. If it casts a silhouette, model it."**

This foundation implements:
- ✅ Hex geometry (modeled - casts silhouettes)
- ✅ Terrain variation (shader-based - procedural)
- ✅ Lighting and materials (shader-based)
- ✅ Edge highlights (shader-based - parchment effect)
- ✅ Era color grading (shader-based - LUT ready)

Future additions will follow the same principle:
- Water ripples → shader animation
- Terrain blending → shader triplanar
- Forest sway → shader animation
- Mountains/buildings → modeled (silhouettes)

---

## 🚀 Getting Started

1. **Build the project:**
```bash
cmake --preset=default
cmake --build build
```

2. **Run shaders compilation:**
```bash
cd shaders
.\compile_shaders.bat
```

3. **Test the foundation:**
   - Open `src/terrain_example.hpp`
   - Modify `initializeSampleTerrain()` to create your terrain
   - Run the application

4. **Experiment:**
   - Change terrain types
   - Adjust camera parameters
   - Modify shader colors/lighting
   - Add more hexes

---

## 📖 References

- **Hex Grids:** https://www.redblobgames.com/grids/hexagons/
- **Art Direction:** `design/art-style.md`
- **Vulkan 1.4 Spec:** https://www.khronos.org/vulkan/
- **GLM Documentation:** https://github.com/g-truc/glm

---

**Foundation Status:** ✅ Complete and Ready for Development

All core systems are in place. You can now:
1. Start implementing advanced rendering features
2. Add gameplay systems (units, cities, etc.)
3. Expand terrain variety and effects
4. Implement post-processing pipeline
5. Add UI and interaction systems

Happy building! 🎨🗺️

