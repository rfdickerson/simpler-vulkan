#pragma once

#include "buffer.hpp"
#include "camera.hpp"
#include "device.hpp"
#include "glyph_atlas.hpp"
#include "hex_coord.hpp"
#include "hex_mesh.hpp"
#include "image.hpp"
#include "map_builder.hpp"
#include "noise.hpp"
#include "render_graph.hpp"
#include "ssao_pipeline.hpp"
#include "swapchain.hpp"
#include "terrain.hpp"
#include "terrain_example.hpp"
#include "terrain_pipeline.hpp"
#include "terrain_renderer.hpp"
#include "text.hpp"
#include "text_pipeline.hpp"
#include "text_renderer_example.hpp"
#include "tiltshift_pipeline.hpp"
#include "tree_pipeline.hpp"
#include "tree_renderer.hpp"
#include "window.hpp"

namespace hex {

using Buffer = ::Buffer;
using Camera = ::Camera;
using Device = ::Device;
using GlyphAtlas = ::GlyphAtlas;
using GlyphInfo = ::GlyphInfo;
using HexCoord = ::HexCoord;
using HexMesh = ::HexMesh;
using Image = ::Image;
using MapBuilder = ::MapBuilder;
using MapConfig = ::MapConfig;
using RenderAttachment = ::RenderAttachment;
using RenderGraph = ::RenderGraph;
using RenderPassDesc = ::RenderPassDesc;
using SSAOPipeline = ::SSAOPipeline;
using SSAOPushConstants = ::SSAOPushConstants;
using Swapchain = ::Swapchain;
using SwapchainImage = ::SwapchainImage;
using Terrain = ::Terrain;
using TerrainExample = ::TerrainExample;
using TerrainPipeline = ::TerrainPipeline;
using TerrainParamsUBO = ::TerrainParamsUBO;
using TerrainPushConstants = ::TerrainPushConstants;
using TerrainRenderer = ::TerrainRenderer;
using TextPipeline = ::TextPipeline;
using TextVertex = ::TextVertex;
using TiltShiftPipeline = ::TiltShiftPipeline;
using TiltShiftPushConstants = ::TiltShiftPushConstants;
using TreePipeline = ::TreePipeline;
using TreeRenderer = ::TreeRenderer;
using Window = ::Window;

using ::initDevice;
using ::initVma;
using ::initVulkan;
using ::makeTimelineSemaphore;
using ::cleanupVma;
using ::cleanupVulkan;

using ::createSurface;
using ::destroySurface;
using ::createSwapchain;
using ::recreateSwapchain;
using ::cleanupSwapchain;
using ::acquireNextImage;
using ::presentImage;

} // namespace hex

