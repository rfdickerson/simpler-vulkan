#pragma once

#include <vector>
#include <memory>
#include <chrono>

#include "window.hpp"
#include "device.hpp"
#include "swapchain.hpp"
#include "render_graph.hpp"
#include "terrain_example.hpp"
#include "camera_controller.hpp"

class TerrainApplication {
public:
    void initialize();
    void run();
    void shutdown();

private:
    void recreateSwapchainResources();

    Window window{};
    Device device{};
    VkSurfaceKHR surface{VK_NULL_HANDLE};
    Swapchain swapchain{};
    std::unique_ptr<TerrainExample> terrainExample{};

    VkCommandPool commandPool{VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> commandBuffers{};

    RenderGraph renderGraph{};
    CameraController cameraController{};
    bool framebufferResized{false};
    std::chrono::high_resolution_clock::time_point lastFrameTime{};
};
