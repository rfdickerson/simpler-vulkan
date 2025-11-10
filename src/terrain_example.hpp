#pragma once

#include "device.hpp"
#include "swapchain.hpp"
#include "terrain_renderer.hpp"
#include "terrain_pipeline.hpp"
#include "camera.hpp"

// Example terrain scene setup
class TerrainExample {
public:
    TerrainExample(Device& device, Swapchain& swapchain)
        : device(device)
        , swapchain(swapchain)
        , terrainRenderer(device, 1.0f) // Hex size of 1.0
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
        
        // Create pipeline
        createTerrainPipeline(device, swapchain, pipeline);
        createTerrainCommandBuffers(device, pipeline, swapchain.MAX_FRAMES_IN_FLIGHT);
        
        std::cout << "Terrain example initialized." << std::endl;
    }
    
    ~TerrainExample() {
        destroyTerrainPipeline(device, pipeline);
    }
    
    void initializeSampleTerrain() {
        // Create a small radial grid for testing
        terrainRenderer.initializeRadialGrid(HexCoord(0, 0), 8);
        
        // Add some variety
        terrainRenderer.setTerrainType(HexCoord(0, 0), TerrainType::Plains);
        terrainRenderer.setTerrainType(HexCoord(2, 0), TerrainType::Hills);
        terrainRenderer.setTerrainHeight(HexCoord(2, 0), 0.5f);
        terrainRenderer.setTerrainType(HexCoord(-2, 2), TerrainType::Forest);
        terrainRenderer.setTerrainType(HexCoord(1, -2), TerrainType::Mountains);
        terrainRenderer.setTerrainHeight(HexCoord(1, -2), 1.0f);
        
        // Build the mesh
        terrainRenderer.rebuildMesh();
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
    
    void render(VkCommandBuffer cmd) {
        // Bind pipeline
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);
        
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
        
        // Push constants
        TerrainPushConstants pushConstants;
        pushConstants.viewProj = camera.getViewProjectionMatrix();
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
        
        // Draw
        vkCmdDrawIndexed(cmd, terrainRenderer.getIndexCount(), 1, 0, 0, 0);
    }
    
    Camera& getCamera() { return camera; }
    
private:
    Device& device;
    Swapchain& swapchain;
    TerrainRenderer terrainRenderer;
    TerrainPipeline pipeline;
    Camera camera;
    float elapsedTime = 0.0f;
};

