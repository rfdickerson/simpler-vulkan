#include <vulkan/vulkan.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstdint>
#include <memory>
#include <chrono>

#include "device.hpp"
#include "buffer.hpp"
#include "window.hpp"
#include "text.hpp"
#include "glyph_atlas.hpp"
#include "swapchain.hpp"
#include "text_pipeline.hpp"
#include "terrain_example.hpp"
#include <glm/glm.hpp>

// Colored vertex structure for the triangle
struct ColoredVertex {
    float pos[2];    // Position in NDC
    float color[3];  // RGB color
};

// Simple pipeline structure for the triangle
struct TrianglePipeline {
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;
};

// Create triangle rendering pipeline
void createTrianglePipeline(Device& device, Swapchain& swapchain, TrianglePipeline& pipeline) {
    // Load shaders
    VkShaderModule vertShaderModule = loadShaderModule(device, "../shaders/triangle.vert.spv");
    VkShaderModule fragShaderModule = loadShaderModule(device, "../shaders/triangle.frag.spv");

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // Vertex input
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(ColoredVertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(ColoredVertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(ColoredVertex, color);

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor (dynamic)
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // Rasterization
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Blending (no blending for solid triangle)
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Dynamic states
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // Pipeline layout (no descriptors or push constants needed)
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    if (vkCreatePipelineLayout(device.device, &pipelineLayoutInfo, nullptr, &pipeline.pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create triangle pipeline layout!");
    }

    // Dynamic rendering info
    VkFormat colorFormat = swapchain.format;
    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &colorFormat;

    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &renderingInfo;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipeline.pipelineLayout;
    pipelineInfo.renderPass = VK_NULL_HANDLE;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline.pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create triangle graphics pipeline!");
    }

    // Cleanup shader modules
    vkDestroyShaderModule(device.device, vertShaderModule, nullptr);
    vkDestroyShaderModule(device.device, fragShaderModule, nullptr);

    std::cout << "Triangle pipeline created successfully." << std::endl;
}

void destroyTrianglePipeline(Device& device, TrianglePipeline& pipeline) {
    if (pipeline.pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device.device, pipeline.pipeline, nullptr);
        pipeline.pipeline = VK_NULL_HANDLE;
    }
    if (pipeline.pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device.device, pipeline.pipelineLayout, nullptr);
        pipeline.pipelineLayout = VK_NULL_HANDLE;
    }
}

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
        window.CreateWindow(1280, 720, "Hex Terrain Renderer");

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

        // Create terrain example
        std::unique_ptr<TerrainExample> terrainExample = std::make_unique<TerrainExample>(device, swapchain);

        // Create command pool and buffers
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = 0;

        VkCommandPool commandPool;
        if (vkCreateCommandPool(device.device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool!");
        }

        std::vector<VkCommandBuffer> commandBuffers(swapchain.MAX_FRAMES_IN_FLIGHT);
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
        vkAllocateCommandBuffers(device.device, &allocInfo, commandBuffers.data());

        std::cout << "Terrain scene ready to render..." << std::endl;

        bool framebufferResized = false;
        
        // Time tracking for deltaTime
        auto lastFrameTime = std::chrono::high_resolution_clock::now();

        // Main render loop
        while (!window.shouldClose()) {
            window.pollEvents();

            // Calculate deltaTime
            auto currentFrameTime = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float>(currentFrameTime - lastFrameTime).count();
            lastFrameTime = currentFrameTime;

			// Apply camera panning from right-mouse drag
			{
				float panDX = 0.0f, panDY = 0.0f;
				if (window.consumeCameraPanDelta(panDX, panDY)) {
					Camera& cam = terrainExample->getCamera();
					glm::vec3 viewDir = glm::normalize(cam.target - cam.position);
					glm::vec2 forwardXZ = glm::normalize(glm::vec2(viewDir.x, viewDir.z));
					glm::vec2 rightXZ = glm::vec2(forwardXZ.y, -forwardXZ.x);

					float sensitivity = 0.0025f * cam.orbitRadius;
					float dxWorld = (-panDX) * sensitivity * rightXZ.x + (panDY) * sensitivity * forwardXZ.x;
					float dzWorld = (-panDX) * sensitivity * rightXZ.y + (panDY) * sensitivity * forwardXZ.y;
					cam.pan(dxWorld, dzWorld);
				}
			}

            // Handle window resize
            if (framebufferResized) {
                recreateSwapchain(device, surface, window, swapchain);
                terrainExample->getCamera().setAspectRatio(
                    static_cast<float>(swapchain.extent.width) / 
                    static_cast<float>(swapchain.extent.height));
                framebufferResized = false;
            }

            // Update terrain scene
            terrainExample->update(deltaTime);

            // Acquire next image
            if (!acquireNextImage(device, swapchain)) {
                recreateSwapchain(device, surface, window, swapchain);
                terrainExample->getCamera().setAspectRatio(
                    static_cast<float>(swapchain.extent.width) / 
                    static_cast<float>(swapchain.extent.height));
                continue;
            }

            // Record command buffer
            VkCommandBuffer cmd = commandBuffers[swapchain.currentFrame];
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
            colorAttachment.clearValue.color = {{0.05f, 0.05f, 0.08f, 1.0f}};

            VkRenderingInfo renderingInfo{};
            renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            renderingInfo.renderArea.offset = {0, 0};
            renderingInfo.renderArea.extent = swapchain.extent;
            renderingInfo.layerCount = 1;
            renderingInfo.colorAttachmentCount = 1;
            renderingInfo.pColorAttachments = &colorAttachment;

            vkCmdBeginRendering(cmd, &renderingInfo);

            // Render terrain
            terrainExample->render(cmd);

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
            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            
            // Wait on imageAvailable (indexed by currentFrame, from acquire)
            VkSemaphore waitSemaphores[] = {swapchain.imageAvailableSemaphores[swapchain.currentFrame]};
            VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = waitSemaphores;
            submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &cmd;
            
            // Signal renderFinished (indexed by currentImageIndex, for present)
            VkSemaphore signalSemaphores[] = {swapchain.renderFinishedSemaphores[swapchain.currentImageIndex]};
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = signalSemaphores;

            if (vkQueueSubmit(device.queue, 1, &submitInfo, swapchain.inFlightFences[swapchain.currentFrame]) != VK_SUCCESS) {
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
        terrainExample.reset(); // Destroy terrain before command pool
        vkDestroyCommandPool(device.device, commandPool, nullptr);
        
        // Destroy imageAvailable semaphores (one per frame-in-flight)
        for (size_t i = 0; i < swapchain.imageAvailableSemaphores.size(); i++) {
            vkDestroySemaphore(device.device, swapchain.imageAvailableSemaphores[i], nullptr);
        }
        
        // Destroy renderFinished semaphores (one per swapchain image)
        for (size_t i = 0; i < swapchain.renderFinishedSemaphores.size(); i++) {
            vkDestroySemaphore(device.device, swapchain.renderFinishedSemaphores[i], nullptr);
        }
        
        // Destroy fences (one per frame-in-flight)
        for (size_t i = 0; i < swapchain.MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroyFence(device.device, swapchain.inFlightFences[i], nullptr);
        }
        
        cleanupSwapchain(device, swapchain);
        destroySurface(device.instance, surface);
        vkDestroySemaphore(device.device, device.timelineSemaphore, nullptr);
        
        cleanupVma(device);
        vkDestroyDevice(device.device, nullptr);
        cleanupVulkan(device);
        window.cleanup();

        std::cout << "Terrain renderer closed successfully." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
