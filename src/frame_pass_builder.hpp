#pragma once

#include "render_graph.hpp"
#include "swapchain.hpp"
#include "terrain_example.hpp"

namespace FramePassBuilder {
RenderPassDesc buildDepthPrepass(const Swapchain& swapchain, TerrainExample& terrainExample);
RenderPassDesc buildSsaoPass(const Swapchain& swapchain, TerrainExample& terrainExample);
RenderPassDesc buildTerrainPass(const Swapchain& swapchain, TerrainExample& terrainExample);
RenderPassDesc buildTiltShiftPass(const Swapchain& swapchain, TerrainExample& terrainExample, uint32_t imageIndex);
}
