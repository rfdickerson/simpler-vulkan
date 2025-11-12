#include <vulkan/vulkan.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstdint>
#include <memory>
#include <chrono>
#include <thread>

#include "device.hpp"
#include "buffer.hpp"
#include "window.hpp"
#include "text.hpp"
#include "swapchain.hpp"
#include "text_pipeline.hpp"
#include "terrain_example.hpp"
#include "ui_renderer.hpp"
#include <glm/glm.hpp>
#include "render_graph.hpp"
#include "hex_coord.hpp"

#include <array>
#include <cmath>

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

        // Initialize UI renderer with HarfBuzz shaping and SDF atlas
        ui::UiRenderer uiRenderer(device, swapchain, "assets/fonts/EBGaramond-Regular.ttf", 42);

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

        float uiTimeline = 0.0f;

        // Main render loop
        while (!window.shouldClose()) {
            window.pollEvents();

            // Calculate deltaTime
            auto currentFrameTime = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float>(currentFrameTime - lastFrameTime).count();
            lastFrameTime = currentFrameTime;
            uiTimeline += deltaTime;

			// Apply camera panning from middle-mouse drag
			{
				float panDX = 0.0f, panDY = 0.0f;
				if (window.consumeCameraPanDelta(panDX, panDY)) {
					Camera& cam = terrainExample->getCamera();
					glm::vec3 viewDir = glm::normalize(cam.target - cam.position);
					glm::vec2 forwardXZ = glm::normalize(glm::vec2(viewDir.x, viewDir.z));
					glm::vec2 rightXZ = glm::vec2(forwardXZ.y, -forwardXZ.x);

					float sensitivity = 0.0025f * cam.orbitRadius;
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
                        float rotateSensitivityDeg = 5.0f;
                        cam.rotate(-scrollY * rotateSensitivityDeg);
                    } else {
                        float zoomSensitivity = 1.0f;
                        cam.zoom(-scrollY * zoomSensitivity);
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

                    float baseSpeed = 5.0f;
                    float speed = baseSpeed * deltaTime * glm::max(cam.orbitRadius * 0.1f, 1.0f);

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
                recreateSwapchain(device, surface, window, swapchain);
                graph.resetLayoutTracking(); // Clear layout tracking for new swapchain images
                terrainExample->getCamera().setAspectRatio(
                    static_cast<float>(swapchain.extent.width) / 
                    static_cast<float>(swapchain.extent.height));
                // Rebind dynamic image descriptors
                terrainExample->rebindSsaoDescriptors();
                uiRenderer.onSwapchainResized(swapchain);
                framebufferResized = false;
            }

            // Update terrain scene
            terrainExample->update(deltaTime);

            // Build UI for this frame
            uiRenderer.beginFrame(swapchain.extent);

            const ui::PanelStyle& basePanelStyle = uiRenderer.defaultPanelStyle();
            const ui::ButtonStyle& baseButtonStyle = uiRenderer.defaultButtonStyle();
            ui::LabelStyle baseLabelStyle = uiRenderer.defaultLabelStyle();

            ui::PanelStyle topBarStyle = basePanelStyle;
            topBarStyle.cornerRadius = 28.0f;
            topBarStyle.shadow.spread = 34.0f;
            topBarStyle.shadow.softness = 24.0f;
            glm::vec2 topBarPos(40.0f, 32.0f);
            glm::vec2 topBarSize(static_cast<float>(swapchain.extent.width) - 80.0f, 128.0f);
            uiRenderer.drawPanel(topBarPos, topBarSize, topBarStyle);

            ui::LabelStyle titleStyle = baseLabelStyle;
            titleStyle.fontSize = 44.0f;
            uiRenderer.drawLabel(topBarPos + glm::vec2(48.0f, 70.0f), "Empire of Aurelia", titleStyle, ui::TextAlign::Left);

            ui::LabelStyle seasonStyle = baseLabelStyle;
            seasonStyle.fontSize = 24.0f;
            seasonStyle.color = glm::vec4(0.75f, 0.78f, 0.88f, 1.0f);
            uiRenderer.drawLabel(topBarPos + glm::vec2(topBarSize.x - 220.0f, 48.0f),
                                 "Autumn, 1550 ACR", seasonStyle, ui::TextAlign::Right);

            ui::LabelStyle metricLabel = seasonStyle;
            metricLabel.fontSize = 22.0f;
            ui::LabelStyle metricValue = baseLabelStyle;
            metricValue.fontSize = 30.0f;
            metricValue.color = glm::vec4(0.96f, 0.86f, 0.60f, 1.0f);

            std::array<std::pair<std::string, std::string>, 4> metrics = {{
                {"Gold", "12.5k"},
                {"Influence", "+38"},
                {"Manpower", "48.2k"},
                {"Stability", "+2"}
            }};
            float metricStride = (topBarSize.x - 320.0f) / static_cast<float>(metrics.size());
            for (size_t i = 0; i < metrics.size(); ++i) {
                float anchorX = topBarPos.x + 320.0f + metricStride * static_cast<float>(i);
                uiRenderer.drawLabel(glm::vec2(anchorX, topBarPos.y + 58.0f),
                                     metrics[i].first, metricLabel, ui::TextAlign::Center);
                uiRenderer.drawLabel(glm::vec2(anchorX, topBarPos.y + 92.0f),
                                     metrics[i].second, metricValue, ui::TextAlign::Center);
            }

            ui::PanelStyle councilPanel = basePanelStyle;
            councilPanel.fillColor = glm::vec4(0.09f, 0.10f, 0.15f, 0.94f);
            councilPanel.cornerRadius = 26.0f;
            councilPanel.shadow.offset = glm::vec2(0.0f, 12.0f);
            councilPanel.shadow.softness = 18.0f;
            councilPanel.shadow.opacity = 0.48f;
            glm::vec2 councilPos(40.0f, 196.0f);
            glm::vec2 councilSize(420.0f, static_cast<float>(swapchain.extent.height) - 280.0f);
            uiRenderer.drawPanel(councilPos, councilSize, councilPanel);

            ui::LabelStyle councilHeader = baseLabelStyle;
            councilHeader.fontSize = 32.0f;
            uiRenderer.drawLabel(councilPos + glm::vec2(36.0f, 72.0f), "Imperial Council", councilHeader);

            ui::LabelStyle councilEntry = baseLabelStyle;
            councilEntry.fontSize = 24.0f;
            councilEntry.color = glm::vec4(0.82f, 0.82f, 0.88f, 1.0f);
            std::array<std::string, 4> agenda = {
                "• Charter the Southern Sea Company",
                "• Fortify the Amberway frontier",
                "• Commission royal academies (+3 science)",
                "• Ratify the coastal trade pact"
            };
            for (size_t i = 0; i < agenda.size(); ++i) {
                float y = councilPos.y + 128.0f + static_cast<float>(i) * 48.0f;
                uiRenderer.drawLabel(glm::vec2(councilPos.x + 44.0f, y), agenda[i], councilEntry, ui::TextAlign::Left);
            }

            ui::PanelStyle eventPanel = basePanelStyle;
            eventPanel.cornerRadius = 24.0f;
            eventPanel.shadow.offset = glm::vec2(0.0f, 10.0f);
            eventPanel.shadow.softness = 18.0f;
            glm::vec2 eventPos(static_cast<float>(swapchain.extent.width) - 440.0f, 200.0f);
            glm::vec2 eventSize(400.0f, 260.0f);
            uiRenderer.drawPanel(eventPos, eventSize, eventPanel);

            ui::LabelStyle eventHeader = baseLabelStyle;
            eventHeader.fontSize = 30.0f;
            uiRenderer.drawLabel(eventPos + glm::vec2(32.0f, 72.0f), "World Events", eventHeader);

            ui::LabelStyle eventBody = baseLabelStyle;
            eventBody.fontSize = 22.0f;
            eventBody.color = glm::vec4(0.78f, 0.80f, 0.89f, 1.0f);
            uiRenderer.drawLabel(eventPos + glm::vec2(32.0f, 124.0f),
                                 "• Northern leagues consolidate trade power.", eventBody);
            uiRenderer.drawLabel(eventPos + glm::vec2(32.0f, 164.0f),
                                 "• Border tensions ease after envoy visit.", eventBody);
            uiRenderer.drawLabel(eventPos + glm::vec2(32.0f, 204.0f),
                                 "• Scholars propose a grand mapping expedition.", eventBody);

            std::array<std::string, 3> actions = {"Issue Edict", "Raise Levies", "Negotiate Treaty"};
            glm::vec2 buttonSize(260.0f, 90.0f);
            float totalButtonsWidth = static_cast<float>(actions.size()) * buttonSize.x +
                                      static_cast<float>(actions.size() - 1) * 28.0f;
            float buttonStartX = (static_cast<float>(swapchain.extent.width) - totalButtonsWidth) * 0.5f;
            float buttonY = static_cast<float>(swapchain.extent.height) - 140.0f;
            size_t highlightIndex = static_cast<size_t>(std::fmod(uiTimeline * 0.5f, static_cast<float>(actions.size())));
            for (size_t i = 0; i < actions.size(); ++i) {
                glm::vec2 pos(buttonStartX + static_cast<float>(i) * (buttonSize.x + 28.0f), buttonY);
                bool hovered = (i == highlightIndex);
                bool pressed = hovered && std::fmod(uiTimeline, 1.2f) > 0.9f;
                uiRenderer.drawButton(pos, buttonSize, actions[i], baseButtonStyle, hovered, pressed, true);
            }

            ui::LabelStyle footerStyle = baseLabelStyle;
            footerStyle.fontSize = 20.0f;
            footerStyle.color = glm::vec4(0.78f, 0.82f, 0.92f, 1.0f);
            uiRenderer.drawLabel(glm::vec2(48.0f, static_cast<float>(swapchain.extent.height) - 56.0f),
                                 "Next harvest festival in 23 days • War exhaustion 12%", footerStyle, ui::TextAlign::Left);

            // Acquire next image
            if (!acquireNextImage(device, swapchain)) {
                recreateSwapchain(device, surface, window, swapchain);
                graph.resetLayoutTracking(); // Clear layout tracking for new swapchain images
                terrainExample->getCamera().setAspectRatio(
                    static_cast<float>(swapchain.extent.width) / 
                    static_cast<float>(swapchain.extent.height));
                // Rebind dynamic image descriptors
                terrainExample->rebindSsaoDescriptors();
                uiRenderer.onSwapchainResized(swapchain);
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

            // UI overlay pass - composited last onto swapchain image
            RenderAttachment uiAtt{};
            uiAtt.extent = swapchain.extent;
            uiAtt.samples = VK_SAMPLE_COUNT_1_BIT;
            uiAtt.colorFormat = swapchain.format;
            uiAtt.colorView = swapchain.images[swapchain.currentImageIndex].view;
            uiAtt.colorImage = swapchain.images[swapchain.currentImageIndex].image;

            RenderPassDesc uiPass{};
            uiPass.name = "ui_overlay";
            uiPass.attachments = uiAtt;
            uiPass.colorLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            uiPass.record = [&](VkCommandBuffer c) {
                uiRenderer.flush(c);
            };

            graph.addPass(uiPass);
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
