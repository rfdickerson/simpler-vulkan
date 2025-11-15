#include <vulkan/vulkan.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstdint>
#include <memory>
#include <chrono>
#include <thread>

#include "firmament/device.hpp"
#include "firmament/buffer.hpp"
#include "firmament/window.hpp"
#include "firmament/text.hpp"
#include "firmament/glyph_atlas.hpp"
#include "firmament/swapchain.hpp"
#include "firmament/text_pipeline.hpp"
#include "terrain_example.hpp"
#include <glm/glm.hpp>
#include "firmament/render_graph.hpp"
#include "firmament/hex_coord.hpp"

using namespace firmament;

// Camera control constants
namespace CameraConstants {
    constexpr float PAN_SENSITIVITY = 0.0025f;
    constexpr float ROTATE_SENSITIVITY_DEG = 5.0f;
    constexpr float ZOOM_SENSITIVITY = 1.0f;
    constexpr float WASD_BASE_SPEED = 5.0f;
    constexpr float WASD_SPEED_SCALE = 0.1f;
}

// Helper function to handle swapchain recreation and related updates
void handleSwapchainRecreation(Device& device, VkSurfaceKHR surface, Window& window,
                               Swapchain& swapchain, RenderGraph& graph,
                               TerrainExample& terrainExample) {
    recreateSwapchain(device, surface, window, swapchain);
    graph.resetLayoutTracking();
    terrainExample.getCamera().setAspectRatio(
        static_cast<float>(swapchain.extent.width) /
        static_cast<float>(swapchain.extent.height));
    terrainExample.rebindSsaoDescriptors();
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

        // Create persistent render graph to track image layouts across frames
        RenderGraph graph;

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

			// Apply camera panning from middle-mouse drag
			{
				float panDX = 0.0f, panDY = 0.0f;
				if (window.consumeCameraPanDelta(panDX, panDY)) {
					Camera& cam = terrainExample->getCamera();
					glm::vec3 viewDir = glm::normalize(cam.target - cam.position);
					glm::vec2 forwardXZ = glm::normalize(glm::vec2(viewDir.x, viewDir.z));
					glm::vec2 rightXZ = glm::vec2(forwardXZ.y, -forwardXZ.x);

					float sensitivity = CameraConstants::PAN_SENSITIVITY * cam.orbitRadius;
					float dxWorld = (-panDX) * sensitivity * rightXZ.x + (-panDY) * sensitivity * forwardXZ.x;
					float dzWorld = (-panDX) * sensitivity * rightXZ.y + (-panDY) * sensitivity * forwardXZ.y;
					cam.pan(dxWorld, dzWorld);
				}
			}

            // Apply camera zoom or rotate via scroll (Alt + Scroll to rotate)
            {
                float scrollX = 0.0f, scrollY = 0.0f;
                if (window.consumeScrollDelta(scrollX, scrollY)) {
                    Camera& cam = terrainExample->getCamera();
                    bool altDown = window.isKeyDown(GLFW_KEY_LEFT_ALT) || window.isKeyDown(GLFW_KEY_RIGHT_ALT);
                    if (altDown) {
                        cam.rotate(-scrollY * CameraConstants::ROTATE_SENSITIVITY_DEG);
                    } else {
                        cam.zoom(-scrollY * CameraConstants::ZOOM_SENSITIVITY);
                    }
                }
            }

            // Apply camera panning via WASD
            {
                int moveForward = window.isKeyDown(GLFW_KEY_W) ? 1 : 0;
                int moveBackward = window.isKeyDown(GLFW_KEY_S) ? 1 : 0;
                int moveLeft = window.isKeyDown(GLFW_KEY_A) ? 1 : 0;
                int moveRight = window.isKeyDown(GLFW_KEY_D) ? 1 : 0;

                int forwardAxis = moveForward - moveBackward;
                int strafeAxis = moveRight - moveLeft;

                if (forwardAxis != 0 || strafeAxis != 0) {
                    Camera& cam = terrainExample->getCamera();
                    glm::vec3 viewDir = glm::normalize(cam.target - cam.position);
                    glm::vec2 forwardXZ = glm::normalize(glm::vec2(viewDir.x, viewDir.z));
                    glm::vec2 rightXZ = glm::vec2(forwardXZ.y, -forwardXZ.x);

                    float speed = CameraConstants::WASD_BASE_SPEED * deltaTime *
                                 glm::max(cam.orbitRadius * CameraConstants::WASD_SPEED_SCALE, 1.0f);

                    float dxWorld = (strafeAxis * rightXZ.x + forwardAxis * forwardXZ.x) * speed;
                    float dzWorld = (strafeAxis * rightXZ.y + forwardAxis * forwardXZ.y) * speed;
                    cam.pan(dxWorld, dzWorld);
                }
            }

            // Reset camera with Home key
            {
                if (window.isKeyDown(GLFW_KEY_HOME)) {
                    terrainExample->getCamera().reset();
                }
            }

			// Handle left mouse click to get hex coordinates
			{
				double clickX = 0.0, clickY = 0.0;
				if (window.consumeLeftMouseClick(clickX, clickY)) {
					Camera& cam = terrainExample->getCamera();
					
					// Unproject screen coordinates to world position on ground plane
					glm::vec3 worldPos = cam.unprojectToPlane(
						static_cast<float>(clickX),
						static_cast<float>(clickY),
						static_cast<float>(swapchain.extent.width),
						static_cast<float>(swapchain.extent.height),
						0.0f // Ground plane at y=0
					);
					
					// Convert world position to hex coordinates
					float hexSize = terrainExample->getHexSize();
					HexCoord hex = worldToHex(worldPos, hexSize);
					
					// Print hex coordinates to console
					std::cout << "Clicked hex tile: (" << hex.q << ", " << hex.r << ")" << std::endl;
				}
			}

            // Handle window resize
            if (framebufferResized) {
                handleSwapchainRecreation(device, surface, window, swapchain, graph, *terrainExample);
                framebufferResized = false;
            }

            // Update terrain scene
            terrainExample->update(deltaTime);

            // Acquire next image
            if (!acquireNextImage(device, swapchain)) {
                handleSwapchainRecreation(device, surface, window, swapchain, graph, *terrainExample);
                continue;
            }

            // Record command buffer
            VkCommandBuffer cmd = commandBuffers[swapchain.currentFrame];
            vkResetCommandBuffer(cmd, 0);

            VkCommandBufferBeginInfo cmdBeginInfo{};
            cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            vkBeginCommandBuffer(cmd, &cmdBeginInfo);

            graph.beginFrame(device, swapchain, cmd);

            // Depth prepass - depth only, no color
            RenderAttachment depthAtt{};
            depthAtt.extent = swapchain.extent;
            depthAtt.samples = swapchain.msaaSamples;
            depthAtt.depthFormat = swapchain.depthFormat;
            depthAtt.depthView = swapchain.depthImage.view;
            depthAtt.depthImage = swapchain.depthImage.image;
            // Resolve depth to single-sample for SSAO
            depthAtt.depthResolveView = swapchain.depthResolved.view;
            depthAtt.depthResolveImage = swapchain.depthResolved.image;
            // No color attachments for depth prepass

            RenderPassDesc depthPass{};
            depthPass.name = "depth_prepass";
            depthPass.attachments = depthAtt;
            depthPass.clearDepth = 1.0f;
            depthPass.clearStencil = 0;
            depthPass.depthLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthPass.record = [&](VkCommandBuffer c) {
                terrainExample->renderDepthOnly(c);
            };

            graph.addPass(depthPass);

            // SSAO pass - writes occlusion to R8 texture, reads resolved depth read-only
            RenderAttachment ssaoAtt{};
            ssaoAtt.extent = swapchain.extent;
            ssaoAtt.samples = VK_SAMPLE_COUNT_1_BIT;
            ssaoAtt.colorFormat = swapchain.ssaoFormat;
            ssaoAtt.colorView = swapchain.ssaoImage.view;
            ssaoAtt.colorImage = swapchain.ssaoImage.image;
            // Include resolved depth to transition it to read-only layout for sampling
            ssaoAtt.depthFormat = swapchain.depthFormat;
            ssaoAtt.depthView = swapchain.depthResolved.view;
            ssaoAtt.depthImage = swapchain.depthResolved.image;

            RenderPassDesc ssaoPass{};
            ssaoPass.name = "ssao";
            ssaoPass.attachments = ssaoAtt;
            ssaoPass.clearColor = {{1.0f, 0.0f, 0.0f, 0.0f}}; // occlusion default = 1
            ssaoPass.depthLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            ssaoPass.depthReadOnly = true;
            ssaoPass.record = [&](VkCommandBuffer c) {
                terrainExample->renderSSAO(c);
            };

            graph.addPass(ssaoPass);

            // Main pass - color + depth (load depth from prepass); render into sceneColor for post
            RenderAttachment att{};
            att.extent = swapchain.extent;
            att.samples = swapchain.msaaSamples;
            att.colorFormat = swapchain.format;
            att.depthFormat = swapchain.depthFormat;
            if (swapchain.msaaSamples != VK_SAMPLE_COUNT_1_BIT) {
                att.colorView = swapchain.msaaColor.view;
                att.colorImage = swapchain.msaaColor.image;
                att.resolveView = swapchain.sceneColor.view;
                att.resolveImage = swapchain.sceneColor.image;
            } else {
                att.colorView = swapchain.sceneColor.view;
                att.colorImage = swapchain.sceneColor.image;
            }
            att.depthView = swapchain.depthImage.view;
            att.depthImage = swapchain.depthImage.image;

            RenderPassDesc pass{};
            pass.name = "terrain";
            pass.attachments = att;
            pass.clearColor = {{0.05f, 0.05f, 0.08f, 1.0f}};
            pass.clearDepth = 1.0f;
            pass.clearStencil = 0;
            pass.depthLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // Load depth from prepass
            // Ensure SSAO image is transitioned to SHADER_READ_ONLY for sampling
            pass.sampledImages.push_back(swapchain.ssaoImage.image);
            pass.record = [&](VkCommandBuffer c) {
                terrainExample->render(c);
            };

            graph.addPass(pass);

            // Tilt-shift post-process: write to swapchain, sample sceneColor and depthResolved
            RenderAttachment tiltAtt{};
            tiltAtt.extent = swapchain.extent;
            tiltAtt.samples = VK_SAMPLE_COUNT_1_BIT;
            tiltAtt.colorFormat = swapchain.format;
            tiltAtt.colorView = swapchain.images[swapchain.currentImageIndex].view;
            tiltAtt.colorImage = swapchain.images[swapchain.currentImageIndex].image;
            // Include resolved depth as read-only to manage layout for sampling
            tiltAtt.depthFormat = swapchain.depthFormat;
            tiltAtt.depthView = swapchain.depthResolved.view;
            tiltAtt.depthImage = swapchain.depthResolved.image;

            RenderPassDesc tiltPass{};
            tiltPass.name = "tiltshift";
            tiltPass.attachments = tiltAtt;
            tiltPass.depthLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            tiltPass.depthReadOnly = true;
            // Ensure sceneColor transitions to sampled for this pass
            tiltPass.sampledImages.push_back(swapchain.sceneColor.image);
            tiltPass.record = [&](VkCommandBuffer c) {
                terrainExample->renderTiltShift(c);
            };

            graph.addPass(tiltPass);
            graph.execute();
            graph.endFrame();

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
            uint64_t signalValue = swapchain.nextTimelineValue++;
            VkSemaphore signalSemaphores[] = {
                swapchain.renderFinishedSemaphores[swapchain.currentImageIndex],
                device.timelineSemaphore
            };
            submitInfo.signalSemaphoreCount = 2;
            submitInfo.pSignalSemaphores = signalSemaphores;

            VkTimelineSemaphoreSubmitInfo timelineInfo{};
            timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
            // Provide wait values for all wait semaphores (0 for binary imageAvailable)
            uint64_t waitValues[1] = {0ull};
            timelineInfo.waitSemaphoreValueCount = submitInfo.waitSemaphoreCount;
            timelineInfo.pWaitSemaphoreValues = waitValues;
            // Provide signal values for all signal semaphores (0 for binary renderFinished, N for timeline)
            uint64_t signalValues[2] = {0ull, signalValue};
            timelineInfo.signalSemaphoreValueCount = submitInfo.signalSemaphoreCount;
            timelineInfo.pSignalSemaphoreValues = signalValues;
            submitInfo.pNext = &timelineInfo;

            // Track timeline value for this frame-in-flight so we can wait before reuse
            swapchain.frameTimelineValues[swapchain.currentFrame] = signalValue;

            if (vkQueueSubmit(device.queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
                throw std::runtime_error("Failed to submit draw command buffer!");
            }

            // Present
            if (!presentImage(device, surface, swapchain)) {
                framebufferResized = true;
            }

            // Small sleep to reduce CPU spin and coil whine
            //std::this_thread::sleep_for(std::chrono::milliseconds(2));
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
