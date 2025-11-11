#pragma once

#include "device.hpp"
#include "swapchain.hpp"
#include "terrain_renderer.hpp"
#include "terrain_pipeline.hpp"
#include "tree_renderer.hpp"
#include "tree_pipeline.hpp"
#include "camera.hpp"
#include "map_builder.hpp"

// Example terrain scene setup
class TerrainExample {
public:
    TerrainExample(Device& device, Swapchain& swapchain)
        : device(device)
        , swapchain(swapchain)
        , terrainRenderer(device, 1.0f) // Hex size of 1.0
        , treeRenderer(device)
        , camera()
    {
        // Setup camera for diorama view
        camera.setAspectRatio(static_cast<float>(swapchain.extent.width) / 
                             static_cast<float>(swapchain.extent.height));
        camera.tiltAngle = 60.0f; // Look down at 60 degrees
        camera.orbitRadius = 20.0f;
        camera.focusOn(glm::vec3(0.0f, 0.0f, 0.0f));
        
        // Initialize terrain
        initializeSampleTerrain();
        
        // Create pipelines
        createTerrainPipeline(device, swapchain, pipeline);
        createTerrainCommandBuffers(device, pipeline, swapchain.MAX_FRAMES_IN_FLIGHT);
        createTreePipeline(device, swapchain, treePipeline, swapchain.depthFormat);
        
        // Generate trees on grass tiles
        treeRenderer.generateTrees(terrainRenderer);
        
        std::cout << "Terrain example initialized." << std::endl;
    }
    
    ~TerrainExample() {
        destroyTerrainPipeline(device, pipeline);
        destroyTreePipeline(device, treePipeline);
    }
    
    void initializeSampleTerrain() {
        // Generate realistic terrain using MapBuilder
        MapConfig config;
        config.width = 40;
        config.height = 24;
        config.seed = 42;  // Fixed seed for consistent starting map
        config.waterLevel = 0.42f;
        config.mountainLevel = 0.72f;
        config.hillLevel = 0.58f;
        config.octaves = 5;
        config.frequency = 0.06f;  // Slightly lower for larger continents
        config.persistence = 0.52f;
        config.useMoistureMap = true;
        config.moistureFrequency = 0.10f;
        
        MapBuilder::generateMap(terrainRenderer, config);
    }
    
    void update(float deltaTime) {
        elapsedTime += deltaTime;
        
        // Update terrain rendering parameters
        terrainRenderer.updateRenderParams(camera, elapsedTime);
        
        // Update terrain uniform buffer
        TerrainParamsUBO params;
        auto& renderParams = terrainRenderer.getRenderParams();
        params.sunDirection = renderParams.sunDirection;
        params.sunColor = renderParams.sunColor;
        params.ambientIntensity = renderParams.ambientIntensity;
        params.hexSize = renderParams.hexSize;
        params.currentEra = static_cast<int>(renderParams.currentEra);
        
        updateTerrainParams(pipeline, params);
    }
    
    void renderDepthOnly(VkCommandBuffer cmd) {
        // Set viewport and scissor
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swapchain.extent.width);
        viewport.height = static_cast<float>(swapchain.extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapchain.extent;
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        
        glm::mat4 viewProj = camera.getViewProjectionMatrix();
        
        // ===== Render terrain depth only =====
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.depthOnlyPipeline);
        
        // Push constants (minimal - just viewProj for depth)
        TerrainPushConstants pushConstants;
        pushConstants.viewProj = viewProj;
        pushConstants.cameraPos = camera.position;
        pushConstants.time = elapsedTime;
        
        vkCmdPushConstants(cmd, pipeline.pipelineLayout,
                          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                          0, sizeof(TerrainPushConstants), &pushConstants);
        
        // Bind descriptor set
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline.pipelineLayout, 0, 1,
                              &pipeline.descriptorSet, 0, nullptr);
        
        // Bind vertex and index buffers
        VkBuffer vertexBuffers[] = {terrainRenderer.getVertexBuffer().buffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(cmd, terrainRenderer.getIndexBuffer().buffer, 0, VK_INDEX_TYPE_UINT32);
        
        // Draw terrain
        vkCmdDrawIndexed(cmd, terrainRenderer.getIndexCount(), 1, 0, 0, 0);
        
        // ===== Render trees depth only =====
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, treePipeline.depthOnlyPipeline);
        treeRenderer.render(cmd, treePipeline.pipelineLayout, viewProj);
    }
    
    void render(VkCommandBuffer cmd) {
        // Set viewport and scissor (shared by both pipelines)
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swapchain.extent.width);
        viewport.height = static_cast<float>(swapchain.extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapchain.extent;
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        
        glm::mat4 viewProj = camera.getViewProjectionMatrix();
        
        // ===== Render terrain =====
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);
        
        // Push constants
        TerrainPushConstants pushConstants;
        pushConstants.viewProj = viewProj;
        pushConstants.cameraPos = camera.position;
        pushConstants.time = elapsedTime;
        
        vkCmdPushConstants(cmd, pipeline.pipelineLayout,
                          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                          0, sizeof(TerrainPushConstants), &pushConstants);
        
        // Bind descriptor set
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline.pipelineLayout, 0, 1,
                              &pipeline.descriptorSet, 0, nullptr);
        
        // Bind vertex and index buffers
        VkBuffer vertexBuffers[] = {terrainRenderer.getVertexBuffer().buffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(cmd, terrainRenderer.getIndexBuffer().buffer, 0, VK_INDEX_TYPE_UINT32);
        
        // Draw terrain
        vkCmdDrawIndexed(cmd, terrainRenderer.getIndexCount(), 1, 0, 0, 0);
        
        // ===== Render trees =====
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, treePipeline.pipeline);
        treeRenderer.render(cmd, treePipeline.pipelineLayout, viewProj);
    }
    
    Camera& getCamera() { return camera; }
    float getHexSize() const { return terrainRenderer.getRenderParams().hexSize; }
    
private:
    Device& device;
    Swapchain& swapchain;
    TerrainRenderer terrainRenderer;
    TreeRenderer treeRenderer;
    TerrainPipeline pipeline;
    TreePipeline treePipeline;
    Camera camera;
    float elapsedTime = 0.0f;
};

