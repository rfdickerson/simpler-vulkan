#pragma once

#include "device.hpp"
#include "swapchain.hpp"
#include "terrain_renderer.hpp"
#include "terrain_pipeline.hpp"
#include "tree_renderer.hpp"
#include "tree_pipeline.hpp"
#include "camera.hpp"
#include "map_builder.hpp"
#include "ssao_pipeline.hpp"
#include "tiltshift_pipeline.hpp"

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
        createSSAOPipeline(device, swapchain, ssaoPipeline);
        // Bind SSAO for terrain and depth for SSAO
        updateTerrainSsaoDescriptor(device, pipeline, swapchain.ssaoImage.view, swapchain.ssaoSampler);
        updateSSAODepthDescriptor(device, ssaoPipeline, swapchain);
        // Create tilt-shift pipeline and bind scene/depth
        createTiltShiftPipeline(device, swapchain, tiltPipeline);
        updateTiltShiftDescriptors(device, tiltPipeline, swapchain);
        
        // Generate trees on grass tiles
        treeRenderer.generateTrees(terrainRenderer);
        
        std::cout << "Terrain example initialized." << std::endl;
    }
    
    ~TerrainExample() {
        destroyTerrainPipeline(device, pipeline);
        destroyTreePipeline(device, treePipeline);
        destroySSAOPipeline(device, ssaoPipeline);
        destroyTiltShiftPipeline(device, tiltPipeline);
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
        applyFullscreenViewport(cmd);
        
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
        applyFullscreenViewport(cmd);
        
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

    void renderSSAO(VkCommandBuffer cmd) {
        applyFullscreenViewport(cmd);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ssaoPipeline.pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ssaoPipeline.pipelineLayout, 0, 1, &ssaoPipeline.descriptorSet, 0, nullptr);

        // Push constants
        SSAOPushConstants pc{};
        pc.radius = 4.0f;      // stronger radius for visibility
        pc.bias = 0.010f;      // slightly lower bias
        pc.intensity = 3.0f;   // stronger contrast
        glm::mat4 invProj = glm::inverse(camera.getProjectionMatrix());
        memcpy(pc.invProj, &invProj, sizeof(float) * 16);
        vkCmdPushConstants(cmd, ssaoPipeline.pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SSAOPushConstants), &pc);

        vkCmdDraw(cmd, 3, 1, 0, 0);
    }

    void rebindSsaoDescriptors() {
        updateTerrainSsaoDescriptor(device, pipeline, swapchain.ssaoImage.view, swapchain.ssaoSampler);
        updateSSAODepthDescriptor(device, ssaoPipeline, swapchain);
        updateTiltShiftDescriptors(device, tiltPipeline, swapchain);
    }

    void renderTiltShift(VkCommandBuffer cmd) {
        applyFullscreenViewport(cmd);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, tiltPipeline.pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, tiltPipeline.pipelineLayout, 0, 1, &tiltPipeline.descriptorSet, 0, nullptr);

        // Miniature/diorama look parameters (tilt-shift effect)
        float angleDeg = 0.0f; // horizontal band (try 5-15 for diagonal tilt)
        float angleRad = angleDeg * 0.017453292519943295f;
        TiltShiftPushConstants pc{};
        pc.cosAngle = cosf(angleRad);
        pc.sinAngle = sinf(angleRad);
        pc.focusCenter = 0.5f;       // center of screen in focus (0.4-0.6 recommended)
        pc.bandHalfWidth = 0.18f;    // sharp band width (0.15-0.25 for subtle)
        pc.blurScale = 0.10f;        // blur falloff rate (0.08-0.15)
        pc.maxRadiusPixels = 18.0f;  // max blur radius (12-25 pixels)
        pc.resolution[0] = static_cast<float>(swapchain.extent.width);
        pc.resolution[1] = static_cast<float>(swapchain.extent.height);
        pc.padding = 0.0f;

        vkCmdPushConstants(cmd, tiltPipeline.pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(TiltShiftPushConstants), &pc);
        vkCmdDraw(cmd, 3, 1, 0, 0);
    }
    
    Camera& getCamera() { return camera; }
    float getHexSize() const { return terrainRenderer.getRenderParams().hexSize; }
    
private:
      void applyFullscreenViewport(VkCommandBuffer cmd) const {
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
      }

    Device& device;
    Swapchain& swapchain;
    TerrainRenderer terrainRenderer;
    TreeRenderer treeRenderer;
    TerrainPipeline pipeline;
    TreePipeline treePipeline;
    SSAOPipeline ssaoPipeline;
    TiltShiftPipeline tiltPipeline;
    Camera camera;
    float elapsedTime = 0.0f;
};

