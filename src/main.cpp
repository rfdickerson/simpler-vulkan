#include <vulkan/vulkan.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstdint>

#include "device.hpp"
#include "buffer.hpp"
#include "window.hpp"
#include "text.hpp"
#include "glyph_atlas.hpp"
#include "swapchain.hpp"
#include "text_pipeline.hpp"

// Helper function to build text vertex buffer from shaped glyphs
std::vector<TextVertex> buildTextVertices(const std::vector<ShapedGlyph>& shapedGlyphs,
                                         const GlyphAtlas& atlas,
                                         float startX, float startY) {
    std::vector<TextVertex> vertices;
    float cursorX = startX;
    float cursorY = startY;
    
    for (const auto& sg : shapedGlyphs) {
        const GlyphInfo* info = atlas.getGlyphInfo(sg.glyph_index);
        if (!info || info->width == 0 || info->height == 0) {
            cursorX += sg.x_advance;
            cursorY += sg.y_advance;
            continue;
        }
        
        float x = cursorX + sg.x_offset + info->bearingX;
        float y = cursorY + sg.y_offset - info->bearingY;
        float w = info->width;
        float h = info->height;
        
        // Two triangles per glyph (6 vertices)
        // Triangle 1: top-left, bottom-left, top-right
        vertices.push_back({{x, y}, {info->uvX, info->uvY}});
        vertices.push_back({{x, y + h}, {info->uvX, info->uvY + info->uvH}});
        vertices.push_back({{x + w, y}, {info->uvX + info->uvW, info->uvY}});
        
        // Triangle 2: top-right, bottom-left, bottom-right
        vertices.push_back({{x + w, y}, {info->uvX + info->uvW, info->uvY}});
        vertices.push_back({{x, y + h}, {info->uvX, info->uvY + info->uvH}});
        vertices.push_back({{x + w, y + h}, {info->uvX + info->uvW, info->uvY + info->uvH}});
        
        cursorX += sg.x_advance;
        cursorY += sg.y_advance;
    }
    
    return vertices;
}

int main() {
    try {
        // Initialize window
        Window window;
        window.InitGLFW();
        window.CreateWindow(800, 600, "Text Rendering Demo");

        // Initialize Vulkan
        Device device;
        initVulkan(device);
        
        // Create surface
        VkSurfaceKHR surface = createSurface(device.instance, window);
        
        initDevice(device);
        initVma(device);
        makeTimelineSemaphore(device);

        // Create swapchain
        Swapchain swapchain;
        createSwapchain(device, surface, window, swapchain);

        // Setup text rendering
        GlyphAtlas atlas(device, 2048, 2048);
        atlas.loadFont("../assets/fonts/EBGaramond-Regular.ttf", 64);

        HbShaper shaper("../assets/fonts/EBGaramond-Regular.ttf", 64);
        auto shapedGlyphs = shaper.shape_utf8("Hello, Vulkan Text Rendering!");

        // Add glyphs to atlas
        for (const auto& g : shapedGlyphs) {
            atlas.addGlyph(g.glyph_index);
        }

        // Create command pool and buffer for atlas upload
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = 0;

        VkCommandPool uploadCommandPool;
        if (vkCreateCommandPool(device.device, &poolInfo, nullptr, &uploadCommandPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create upload command pool!");
        }

        VkCommandBuffer uploadCmd;
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = uploadCommandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        vkAllocateCommandBuffers(device.device, &allocInfo, &uploadCmd);

        // Record atlas upload commands
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(uploadCmd, &beginInfo);
        
        atlas.finalizeAtlas(uploadCmd);
        
        vkEndCommandBuffer(uploadCmd);

        // Submit and wait for atlas upload
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &uploadCmd;

        vkQueueSubmit(device.queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(device.queue);

        vkDestroyCommandPool(device.device, uploadCommandPool, nullptr);

        // Create text rendering pipeline
        TextPipeline textPipeline{};
        createTextPipeline(device, swapchain, textPipeline);
        createTextCommandBuffers(device, textPipeline, swapchain.MAX_FRAMES_IN_FLIGHT);

        // Create atlas sampler and update descriptor set
        VkSampler atlasSampler = createAtlasSampler(device);
        updateTextDescriptorSet(device, textPipeline, atlas.getAtlasImage().view, atlasSampler);

        // Build text vertices
        std::vector<TextVertex> textVertices = buildTextVertices(shapedGlyphs, atlas, 50.0f, 300.0f);
        
        // Create vertex buffer
        VkDeviceSize vertexBufferSize = sizeof(TextVertex) * textVertices.size();
        Buffer vertexBuffer = CreateSsboBuffer(device, vertexBufferSize);
        
        // Upload vertex data
        void* data;
        vmaMapMemory(device.allocator, vertexBuffer.allocation, &data);
        memcpy(data, textVertices.data(), vertexBufferSize);
        vmaUnmapMemory(device.allocator, vertexBuffer.allocation);

        std::cout << "Rendering " << textVertices.size() << " vertices..." << std::endl;

        bool framebufferResized = false;

        // Main render loop
        while (!window.shouldClose()) {
            window.pollEvents();

            // Handle window resize
            if (framebufferResized) {
                recreateSwapchain(device, surface, window, swapchain);
                framebufferResized = false;
            }

            // Acquire next image
            if (!acquireNextImage(device, swapchain)) {
                recreateSwapchain(device, surface, window, swapchain);
                continue;
            }

            // Record command buffer
            VkCommandBuffer cmd = textPipeline.commandBuffers[swapchain.currentFrame];
            vkResetCommandBuffer(cmd, 0);

            VkCommandBufferBeginInfo cmdBeginInfo{};
            cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            vkBeginCommandBuffer(cmd, &cmdBeginInfo);

            // Transition swapchain image to color attachment
            VkImageMemoryBarrier2 barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            barrier.srcAccessMask = 0;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier.image = swapchain.images[swapchain.currentImageIndex].image;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;

            VkDependencyInfo dependencyInfo{};
            dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dependencyInfo.imageMemoryBarrierCount = 1;
            dependencyInfo.pImageMemoryBarriers = &barrier;
            vkCmdPipelineBarrier2(cmd, &dependencyInfo);

            // Begin rendering (dynamic rendering)
            VkRenderingAttachmentInfo colorAttachment{};
            colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            colorAttachment.imageView = swapchain.images[swapchain.currentImageIndex].view;
            colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.clearValue.color = {{0.1f, 0.1f, 0.15f, 1.0f}};

            VkRenderingInfo renderingInfo{};
            renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            renderingInfo.renderArea.offset = {0, 0};
            renderingInfo.renderArea.extent = swapchain.extent;
            renderingInfo.layerCount = 1;
            renderingInfo.colorAttachmentCount = 1;
            renderingInfo.pColorAttachments = &colorAttachment;

            vkCmdBeginRendering(cmd, &renderingInfo);

            // Bind pipeline
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, textPipeline.pipeline);

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
            TextPushConstants pushConstants{};
            pushConstants.screenSize[0] = static_cast<float>(swapchain.extent.width);
            pushConstants.screenSize[1] = static_cast<float>(swapchain.extent.height);
            pushConstants.textColor[0] = 1.0f; // R
            pushConstants.textColor[1] = 1.0f; // G
            pushConstants.textColor[2] = 1.0f; // B
            pushConstants.textColor[3] = 1.0f; // A
            vkCmdPushConstants(cmd, textPipeline.pipelineLayout,
                             VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                             0, sizeof(TextPushConstants), &pushConstants);

            // Bind descriptor set
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  textPipeline.pipelineLayout, 0, 1,
                                  &textPipeline.descriptorSet, 0, nullptr);

            // Bind vertex buffer
            VkBuffer vertexBuffers[] = {vertexBuffer.buffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);

            // Draw
            vkCmdDraw(cmd, static_cast<uint32_t>(textVertices.size()), 1, 0, 0);

            vkCmdEndRendering(cmd);

            // Transition to present
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
            barrier.dstAccessMask = 0;
            barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            vkCmdPipelineBarrier2(cmd, &dependencyInfo);

            vkEndCommandBuffer(cmd);

            // Submit command buffer
            VkSubmitInfo submitInfo2{};
            submitInfo2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            
            VkSemaphore waitSemaphores[] = {swapchain.imageAvailableSemaphores[swapchain.currentFrame]};
            VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
            submitInfo2.waitSemaphoreCount = 1;
            submitInfo2.pWaitSemaphores = waitSemaphores;
            submitInfo2.pWaitDstStageMask = waitStages;
            submitInfo2.commandBufferCount = 1;
            submitInfo2.pCommandBuffers = &cmd;
            
            VkSemaphore signalSemaphores[] = {swapchain.renderFinishedSemaphores[swapchain.currentFrame]};
            submitInfo2.signalSemaphoreCount = 1;
            submitInfo2.pSignalSemaphores = signalSemaphores;

            if (vkQueueSubmit(device.queue, 1, &submitInfo2, swapchain.inFlightFences[swapchain.currentFrame]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to submit draw command buffer!");
            }

            // Present
            if (!presentImage(device, surface, swapchain)) {
                framebufferResized = true;
            }
        }

        // Wait for device to finish
        vkDeviceWaitIdle(device.device);

        // Cleanup
        destroyBuffer(device, vertexBuffer);
        vkDestroySampler(device.device, atlasSampler, nullptr);
        destroyTextPipeline(device, textPipeline);
        
        for (size_t i = 0; i < swapchain.MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(device.device, swapchain.imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(device.device, swapchain.renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(device.device, swapchain.inFlightFences[i], nullptr);
        }
        
        cleanupSwapchain(device, swapchain);
        destroySurface(device.instance, surface);
        vkDestroySemaphore(device.device, device.timelineSemaphore, nullptr);
        cleanupVma(device);
        vkDestroyDevice(device.device, nullptr);
        cleanupVulkan(device);
        window.cleanup();

        std::cout << "Application closed successfully." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
