#include "terrain_application.hpp"

#include <stdexcept>
#include <iostream>

#include "frame_pass_builder.hpp"

void TerrainApplication::initialize() {
    window.InitGLFW();
    window.CreateWindow(1280, 720, "Hex Terrain Renderer");

    initVulkan(device);
    surface = createSurface(device.instance, window);

    initDevice(device);
    initVma(device);
    makeTimelineSemaphore(device);

    createSwapchain(device, surface, window, swapchain);

    terrainExample = std::make_unique<TerrainExample>(device, swapchain);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = 0;

    if (vkCreateCommandPool(device.device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool!");
    }

    commandBuffers.resize(Swapchain::MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    if (vkAllocateCommandBuffers(device.device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers!");
    }

    lastFrameTime = std::chrono::high_resolution_clock::now();

    std::cout << "Terrain scene ready to render..." << std::endl;
}

void TerrainApplication::run() {
    while (!window.shouldClose()) {
        Window::pollEvents();

        auto currentFrameTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentFrameTime - lastFrameTime).count();
        lastFrameTime = currentFrameTime;

        cameraController.update(window, terrainExample->getCamera(), deltaTime);
        cameraController.handleClick(window, swapchain, *terrainExample);

        if (framebufferResized) {
            recreateSwapchainResources();
        }

        terrainExample->update(deltaTime);

        if (!acquireNextImage(device, swapchain)) {
            recreateSwapchainResources();
            continue;
        }

        VkCommandBuffer cmd = commandBuffers[swapchain.currentFrame];
        vkResetCommandBuffer(cmd, 0);

        VkCommandBufferBeginInfo cmdBeginInfo{};
        cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if (vkBeginCommandBuffer(cmd, &cmdBeginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin command buffer!");
        }

        renderGraph.beginFrame(device, swapchain, cmd);
        renderGraph.addPass(FramePassBuilder::buildDepthPrepass(swapchain, *terrainExample));
        renderGraph.addPass(FramePassBuilder::buildSsaoPass(swapchain, *terrainExample));
        renderGraph.addPass(FramePassBuilder::buildTerrainPass(swapchain, *terrainExample));
        renderGraph.addPass(FramePassBuilder::buildTiltShiftPass(swapchain, *terrainExample, swapchain.currentImageIndex));
        renderGraph.execute();
        renderGraph.endFrame();

        if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
            throw std::runtime_error("Failed to record command buffer!");
        }

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {swapchain.imageAvailableSemaphores[swapchain.currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;

        uint64_t signalValue = swapchain.nextTimelineValue++;
        VkSemaphore signalSemaphores[] = {
            swapchain.renderFinishedSemaphores[swapchain.currentImageIndex],
            device.timelineSemaphore};
        submitInfo.signalSemaphoreCount = 2;
        submitInfo.pSignalSemaphores = signalSemaphores;

        VkTimelineSemaphoreSubmitInfo timelineInfo{};
        timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        uint64_t waitValues[1] = {0ull};
        timelineInfo.waitSemaphoreValueCount = submitInfo.waitSemaphoreCount;
        timelineInfo.pWaitSemaphoreValues = waitValues;
        uint64_t signalValues[2] = {0ull, signalValue};
        timelineInfo.signalSemaphoreValueCount = submitInfo.signalSemaphoreCount;
        timelineInfo.pSignalSemaphoreValues = signalValues;
        submitInfo.pNext = &timelineInfo;

        swapchain.frameTimelineValues[swapchain.currentFrame] = signalValue;

        if (vkQueueSubmit(device.queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit draw command buffer!");
        }

        if (!presentImage(device, surface, swapchain)) {
            framebufferResized = true;
        }
    }
}

void TerrainApplication::shutdown() {
    vkDeviceWaitIdle(device.device);

    if (terrainExample) {
        terrainExample.reset();
    }

    if (commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device.device, commandPool, nullptr);
        commandPool = VK_NULL_HANDLE;
    }

    for (VkSemaphore semaphore : swapchain.imageAvailableSemaphores) {
        vkDestroySemaphore(device.device, semaphore, nullptr);
    }

    for (VkSemaphore semaphore : swapchain.renderFinishedSemaphores) {
        vkDestroySemaphore(device.device, semaphore, nullptr);
    }

    cleanupSwapchain(device, swapchain);

    if (surface != VK_NULL_HANDLE) {
        destroySurface(device.instance, surface);
        surface = VK_NULL_HANDLE;
    }

    if (device.timelineSemaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(device.device, device.timelineSemaphore, nullptr);
        device.timelineSemaphore = VK_NULL_HANDLE;
    }

    cleanupVma(device);

    if (device.device != VK_NULL_HANDLE) {
        vkDestroyDevice(device.device, nullptr);
        device.device = VK_NULL_HANDLE;
    }

    cleanupVulkan(device);
    window.cleanup();

    std::cout << "Terrain renderer closed successfully." << std::endl;
}

void TerrainApplication::recreateSwapchainResources() {
    recreateSwapchain(device, surface, window, swapchain);
    renderGraph.resetLayoutTracking();
    Camera& camera = terrainExample->getCamera();
    camera.setAspectRatio(static_cast<float>(swapchain.extent.width) /
                           static_cast<float>(swapchain.extent.height));
    terrainExample->rebindSsaoDescriptors();
    framebufferResized = false;
}
