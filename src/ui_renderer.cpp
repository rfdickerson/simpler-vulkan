#include "ui_renderer.hpp"

#include "device.hpp"
#include "swapchain.hpp"

#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace ui {

namespace {

constexpr VkCommandPoolCreateFlags kUploadPoolFlags =
    VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

constexpr VkCommandBufferUsageFlags kOneTimeSubmit = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

} // namespace

UiRenderer::UiRenderer(Device& device, Swapchain& swapchain,
                       const std::string& fontPath, uint32_t baseFontSize)
    : device_(device)
    , swapchain_(swapchain)
    , pipeline_{}
    , uiAtlas_(device)
    , textRenderer_(device, fontPath, baseFontSize)
    , baseFontSize_(baseFontSize) {

    createTextPipeline(device_, swapchain_, pipeline_, 16);

    uiSampler_ = createUiAtlasSampler(device_);
    uiDescriptorSet_ = allocateTextDescriptorSet(device_, pipeline_);
    glyphDescriptorSet_ = allocateTextDescriptorSet(device_, pipeline_);

    vertexBufferSize_ = sizeof(TextVertex) * 65536;
    vertexBuffer_ = CreateVertexBuffer(device_, vertexBufferSize_);

    // Configure default styles reminiscent of 4X strategy UI palettes
    defaultPanelStyle_.fillColor = glm::vec4(0.07f, 0.08f, 0.12f, 0.94f);
    defaultPanelStyle_.borderColor = glm::vec4(0.45f, 0.36f, 0.22f, 1.0f);
    defaultPanelStyle_.highlightColor = glm::vec4(0.98f, 0.88f, 0.55f, 0.28f);
    defaultPanelStyle_.highlightHeight = 0.3f;
    defaultPanelStyle_.cornerRadius = 22.0f;
    defaultPanelStyle_.borderThickness = 5.0f;
    defaultPanelStyle_.shadow.offset = glm::vec2(0.0f, 16.0f);
    defaultPanelStyle_.shadow.spread = 28.0f;
    defaultPanelStyle_.shadow.softness = 22.0f;
    defaultPanelStyle_.shadow.opacity = 0.55f;

    defaultLabelStyle_.color = glm::vec4(0.92f, 0.9f, 0.85f, 1.0f);
    defaultLabelStyle_.fontSize = static_cast<float>(baseFontSize_);

    defaultButtonStyle_.normal.panel = defaultPanelStyle_;
    defaultButtonStyle_.hover.panel = defaultPanelStyle_;
    defaultButtonStyle_.hover.panel.fillColor = glm::vec4(0.10f, 0.12f, 0.16f, 0.96f);
    defaultButtonStyle_.hover.panel.highlightColor = glm::vec4(1.0f, 0.92f, 0.62f, 0.32f);
    defaultButtonStyle_.pressed.panel = defaultPanelStyle_;
    defaultButtonStyle_.pressed.panel.fillColor = glm::vec4(0.05f, 0.06f, 0.09f, 0.95f);
    defaultButtonStyle_.pressed.panel.highlightColor = glm::vec4(0.7f, 0.6f, 0.3f, 0.25f);
    defaultButtonStyle_.disabled.panel = defaultPanelStyle_;
    defaultButtonStyle_.disabled.panel.fillColor = glm::vec4(0.09f, 0.09f, 0.1f, 0.6f);
    defaultButtonStyle_.disabled.textColor = glm::vec4(0.6f, 0.6f, 0.6f, 1.0f);
    defaultButtonStyle_.normal.textColor = defaultLabelStyle_.color;
    defaultButtonStyle_.hover.textColor = glm::vec4(1.0f, 0.95f, 0.8f, 1.0f);
    defaultButtonStyle_.pressed.textColor = glm::vec4(0.9f, 0.85f, 0.7f, 1.0f);
    defaultButtonStyle_.disabled.textColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
    defaultButtonStyle_.padding = glm::vec2(36.0f, 20.0f);
    defaultButtonStyle_.fontSize = static_cast<float>(baseFontSize_);

    pendingAtlasUpload_ = true;
}

UiRenderer::~UiRenderer() {
    if (uiSampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device_.device, uiSampler_, nullptr);
    }
    destroyBuffer(device_, vertexBuffer_);
    destroyTextPipeline(device_, pipeline_);
}

void UiRenderer::beginFrame(const VkExtent2D& extent) {
    screenExtent_ = extent;
    vertices_.clear();
    drawCommands_.clear();
}

void UiRenderer::drawPanel(const glm::vec2& topLeft, const glm::vec2& size,
                           const PanelStyle& style) {
    if (size.x <= 1.0f || size.y <= 1.0f) {
        return;
    }

    auto roundSize = [](float value) {
        return static_cast<uint32_t>(std::max(1.0f, std::round(value)));
    };

    uint32_t panelWidth = roundSize(size.x);
    uint32_t panelHeight = roundSize(size.y);

    if (style.shadow.enabled && style.shadow.opacity > 0.01f) {
        UiAtlasRegion shadowRegion = uiAtlas_.getDropShadow(panelWidth, panelHeight,
                                                            style.cornerRadius,
                                                            style.shadow.spread,
                                                            style.shadow.softness,
                                                            style.shadow.opacity);
        pendingAtlasUpload_ = pendingAtlasUpload_ || !uiAtlas_.isFinalized();

        glm::vec2 shadowPos = topLeft + style.shadow.offset -
                              glm::vec2(shadowRegion.padding.x, shadowRegion.padding.y);

        glm::vec2 shadowSize = size + glm::vec2(shadowRegion.padding.x + shadowRegion.padding.z,
                                                shadowRegion.padding.y + shadowRegion.padding.w);

        appendQuad(shadowRegion, shadowPos, shadowSize, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), uiDescriptorSet_);
    }

    UiAtlasRegion borderRegion = uiAtlas_.getRoundedRect(panelWidth, panelHeight,
                                                         style.cornerRadius,
                                                         style.feather);
    pendingAtlasUpload_ = pendingAtlasUpload_ || !uiAtlas_.isFinalized();

    appendQuad(borderRegion, topLeft, size, style.borderColor, uiDescriptorSet_);

    float innerBorder = std::max(0.0f, style.borderThickness);
    glm::vec2 innerSize = size - glm::vec2(innerBorder) * 2.0f;
    if (innerSize.x > 1.0f && innerSize.y > 1.0f) {
        uint32_t innerWidth = roundSize(innerSize.x);
        uint32_t innerHeight = roundSize(innerSize.y);
        float innerRadius = std::max(0.0f, style.cornerRadius - innerBorder);
        UiAtlasRegion innerRegion = uiAtlas_.getRoundedRect(innerWidth, innerHeight,
                                                            innerRadius,
                                                            style.feather);
        pendingAtlasUpload_ = pendingAtlasUpload_ || !uiAtlas_.isFinalized();

        glm::vec2 innerTopLeft = topLeft + glm::vec2(innerBorder);
        appendQuad(innerRegion, innerTopLeft, innerSize, style.fillColor, uiDescriptorSet_);

        if (style.highlightColor.a > 0.01f && style.highlightHeight > 0.0f) {
            UiAtlasRegion highlightRegion = uiAtlas_.getRoundedRectHighlight(innerWidth, innerHeight,
                                                                             innerRadius, style.feather,
                                                                             style.highlightHeight);
            pendingAtlasUpload_ = pendingAtlasUpload_ || !uiAtlas_.isFinalized();
            appendQuad(highlightRegion, innerTopLeft, innerSize, style.highlightColor, uiDescriptorSet_);
        }
    }
}

void UiRenderer::drawButton(const glm::vec2& topLeft, const glm::vec2& size,
                            const std::string& label, const ButtonStyle& style,
                            bool hovered, bool pressed, bool enabled) {
    const ButtonStateStyle* state = &style.normal;
    if (!enabled) {
        state = &style.disabled;
    } else if (pressed) {
        state = &style.pressed;
    } else if (hovered) {
        state = &style.hover;
    }

    drawPanel(topLeft, size, state->panel);

    if (label.empty()) {
        return;
    }

    auto glyphs = textRenderer_.prepareText(label);
    if (!textRenderer_.isAtlasFinalized()) {
        pendingAtlasUpload_ = true;
    }

    float scale = style.fontSize / static_cast<float>(baseFontSize_);
    TextMetrics metrics = computeTextMetrics(glyphs, scale);

    glm::vec2 contentTopLeft = topLeft + style.padding;
    glm::vec2 contentSize = size - style.padding * 2.0f;
    contentSize = glm::max(contentSize, glm::vec2(4.0f));

    float textWidth = metrics.width;
    float textHeight = metrics.maxY - metrics.minY;

    float anchorX = contentTopLeft.x + (contentSize.x - textWidth) * 0.5f;
    float anchorY = contentTopLeft.y + (contentSize.y - textHeight) * 0.5f;
    glm::vec2 baseline(anchorX, anchorY - metrics.minY);

    appendText(glyphs, baseline, scale, state->textColor);
}

void UiRenderer::drawLabel(const glm::vec2& topLeft, const std::string& text,
                           const LabelStyle& style, TextAlign align) {
    if (text.empty()) {
        return;
    }

    auto glyphs = textRenderer_.prepareText(text);
    if (!textRenderer_.isAtlasFinalized()) {
        pendingAtlasUpload_ = true;
    }

    float scale = style.fontSize / static_cast<float>(baseFontSize_);
    TextMetrics metrics = computeTextMetrics(glyphs, scale);

    float startX = topLeft.x;
    if (align == TextAlign::Center) {
        startX -= metrics.width * 0.5f;
    } else if (align == TextAlign::Right) {
        startX -= metrics.width;
    }

    float baselineY = topLeft.y - metrics.minY;
    appendText(glyphs, glm::vec2(startX, baselineY), scale, style.color);
}

void UiRenderer::ensureAtlasesUploaded() {
    bool uiDirty = !uiAtlas_.isFinalized();
    bool textDirty = !textRenderer_.isAtlasFinalized();

    if (!uiDirty && !textDirty && !pendingAtlasUpload_) {
        return;
    }

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = 0;
    poolInfo.flags = kUploadPoolFlags;

    VkCommandPool uploadPool = VK_NULL_HANDLE;
    if (vkCreateCommandPool(device_.device, &poolInfo, nullptr, &uploadPool) != VK_SUCCESS) {
        throw std::runtime_error("UiRenderer: failed to create upload command pool");
    }

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = uploadPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(device_.device, &allocInfo, &cmd) != VK_SUCCESS) {
        vkDestroyCommandPool(device_.device, uploadPool, nullptr);
        throw std::runtime_error("UiRenderer: failed to allocate upload command buffer");
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = kOneTimeSubmit;
    vkBeginCommandBuffer(cmd, &beginInfo);

    std::vector<Buffer> stagingBuffers;
    stagingBuffers.reserve(2);

    if (uiDirty) {
        stagingBuffers.push_back(uiAtlas_.finalize(cmd));
    }
    if (textDirty) {
        stagingBuffers.push_back(textRenderer_.finalizeAtlas(cmd));
    }

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    if (vkQueueSubmit(device_.queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        vkDestroyCommandPool(device_.device, uploadPool, nullptr);
        throw std::runtime_error("UiRenderer: failed to submit atlas upload commands");
    }

    vkQueueWaitIdle(device_.queue);

    for (Buffer staging : stagingBuffers) {
        destroyBuffer(device_, staging);
    }

    vkDestroyCommandPool(device_.device, uploadPool, nullptr);

    if (uiDirty || pendingAtlasUpload_) {
        updateTextDescriptorSet(device_, uiDescriptorSet_,
                                uiAtlas_.getAtlasImage().view, uiSampler_);
    }
    if (textDirty || pendingAtlasUpload_) {
        updateTextDescriptorSet(device_, glyphDescriptorSet_,
                                textRenderer_.getAtlasImage().view,
                                textRenderer_.getSampler());
    }

    pendingAtlasUpload_ = false;
}

void UiRenderer::ensureVertexCapacity(size_t vertexCount) {
    VkDeviceSize requiredSize = static_cast<VkDeviceSize>(vertexCount * sizeof(TextVertex));
    if (requiredSize <= vertexBufferSize_) {
        return;
    }

    destroyBuffer(device_, vertexBuffer_);
    vertexBufferSize_ = std::max(requiredSize, vertexBufferSize_ * 2);
    vertexBuffer_ = CreateVertexBuffer(device_, vertexBufferSize_);
}

glm::vec2 UiRenderer::toBottomLeft(const glm::vec2& topLeft, float height) const {
    return glm::vec2(topLeft.x,
                     static_cast<float>(screenExtent_.height) - (topLeft.y + height));
}

void UiRenderer::appendQuad(const UiAtlasRegion& region, const glm::vec2& topLeft,
                            const glm::vec2& size, const glm::vec4& color,
                            VkDescriptorSet descriptorSet) {
    glm::vec2 bottomLeft = toBottomLeft(topLeft, size.y);

    float x0 = bottomLeft.x;
    float y0 = bottomLeft.y;
    float x1 = x0 + size.x;
    float y1 = y0 + size.y;

    float u0 = region.uvX;
    float v0 = region.uvY;
    float u1 = region.uvX + region.uvW;
    float v1 = region.uvY + region.uvH;

    TextVertex v0Vert{{x0, y0}, {u0, v0}};
    TextVertex v1Vert{{x0, y1}, {u0, v1}};
    TextVertex v2Vert{{x1, y0}, {u1, v0}};
    TextVertex v3Vert{{x1, y1}, {u1, v1}};

    uint32_t first = static_cast<uint32_t>(vertices_.size());

    vertices_.push_back(v0Vert);
    vertices_.push_back(v1Vert);
    vertices_.push_back(v2Vert);
    vertices_.push_back(v2Vert);
    vertices_.push_back(v1Vert);
    vertices_.push_back(v3Vert);

    DrawCommand cmd{};
    cmd.descriptorSet = descriptorSet;
    cmd.color = color;
    cmd.firstVertex = first;
    cmd.vertexCount = 6;
    drawCommands_.push_back(cmd);
}

UiRenderer::TextMetrics UiRenderer::computeTextMetrics(const std::vector<ShapedGlyph>& glyphs,
                                                       float scale) const {
    TextMetrics metrics{};
    if (glyphs.empty()) {
        return metrics;
    }

    float cursorX = 0.0f;
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    bool hasGlyph = false;

    for (const auto& sg : glyphs) {
        const GlyphInfo* info = textRenderer_.getGlyphInfo(sg.glyph_index);
        if (!info) {
            cursorX += sg.x_advance * scale;
            continue;
        }

        float x = cursorX + sg.x_offset * scale + info->bearingX * scale;
        float y = sg.y_offset * scale - info->bearingY * scale;
        float h = info->height * scale;

        minY = std::min(minY, y);
        maxY = std::max(maxY, y + h);
        cursorX += sg.x_advance * scale;
        hasGlyph = true;
    }

    metrics.width = cursorX;
    if (hasGlyph) {
        metrics.minY = minY;
        metrics.maxY = maxY;
    } else {
        metrics.minY = -static_cast<float>(baseFontSize_) * scale * 0.8f;
        metrics.maxY = 0.0f;
    }

    return metrics;
}

void UiRenderer::appendText(const std::vector<ShapedGlyph>& glyphs,
                            const glm::vec2& baselineTopLeft,
                            float scale,
                            const glm::vec4& color) {
    if (glyphs.empty()) {
        return;
    }

    uint32_t first = static_cast<uint32_t>(vertices_.size());
    float cursorX = baselineTopLeft.x;
    float cursorY = baselineTopLeft.y;

    for (const auto& sg : glyphs) {
        const GlyphInfo* info = textRenderer_.getGlyphInfo(sg.glyph_index);
        if (!info || info->width == 0 || info->height == 0) {
            cursorX += sg.x_advance * scale;
            cursorY += sg.y_advance * scale;
            continue;
        }

        float x = cursorX + sg.x_offset * scale + info->bearingX * scale;
        float y = cursorY + sg.y_offset * scale - info->bearingY * scale;
        float w = info->width * scale;
        float h = info->height * scale;

        glm::vec2 bottomLeft = toBottomLeft(glm::vec2(x, y), h);

        float u0 = info->uvX;
        float v0 = info->uvY;
        float u1 = info->uvX + info->uvW;
        float v1 = info->uvY + info->uvH;

        TextVertex v0Vert{{bottomLeft.x, bottomLeft.y}, {u0, v0}};
        TextVertex v1Vert{{bottomLeft.x, bottomLeft.y + h}, {u0, v1}};
        TextVertex v2Vert{{bottomLeft.x + w, bottomLeft.y}, {u1, v0}};
        TextVertex v3Vert{{bottomLeft.x + w, bottomLeft.y + h}, {u1, v1}};

        vertices_.push_back(v0Vert);
        vertices_.push_back(v1Vert);
        vertices_.push_back(v2Vert);
        vertices_.push_back(v2Vert);
        vertices_.push_back(v1Vert);
        vertices_.push_back(v3Vert);

        cursorX += sg.x_advance * scale;
        cursorY += sg.y_advance * scale;
    }

    uint32_t count = static_cast<uint32_t>(vertices_.size()) - first;
    if (count == 0) {
        return;
    }

    DrawCommand cmd{};
    cmd.descriptorSet = glyphDescriptorSet_;
    cmd.color = color;
    cmd.firstVertex = first;
    cmd.vertexCount = count;
    drawCommands_.push_back(cmd);
}

void UiRenderer::flush(VkCommandBuffer cmd) {
    ensureAtlasesUploaded();

    if (vertices_.empty()) {
        return;
    }

    ensureVertexCapacity(vertices_.size());

    void* mapped = nullptr;
    VkResult mapRes = vmaMapMemory(device_.allocator, vertexBuffer_.allocation, &mapped);
    if (mapRes != VK_SUCCESS) {
        throw std::runtime_error("UiRenderer: failed to map vertex buffer");
    }
    std::memcpy(mapped, vertices_.data(), vertices_.size() * sizeof(TextVertex));
    vmaUnmapMemory(device_.allocator, vertexBuffer_.allocation);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.pipeline);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(screenExtent_.width);
    viewport.height = static_cast<float>(screenExtent_.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = screenExtent_;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    VkBuffer buffers[] = {vertexBuffer_.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);

    for (const DrawCommand& draw : drawCommands_) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline_.pipelineLayout, 0, 1,
                                &draw.descriptorSet, 0, nullptr);

        TextPushConstants push{};
        push.screenSize[0] = static_cast<float>(screenExtent_.width);
        push.screenSize[1] = static_cast<float>(screenExtent_.height);
        push.textColor[0] = draw.color.r;
        push.textColor[1] = draw.color.g;
        push.textColor[2] = draw.color.b;
        push.textColor[3] = draw.color.a;

        vkCmdPushConstants(cmd, pipeline_.pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(TextPushConstants), &push);

        vkCmdDraw(cmd, draw.vertexCount, 1, draw.firstVertex, 0);
    }
}

void UiRenderer::onSwapchainResized(Swapchain& swapchain) {
    destroyTextPipeline(device_, pipeline_);
    createTextPipeline(device_, swapchain, pipeline_, 16);

    uiDescriptorSet_ = allocateTextDescriptorSet(device_, pipeline_);
    glyphDescriptorSet_ = allocateTextDescriptorSet(device_, pipeline_);

    pendingAtlasUpload_ = true;
}

} // namespace ui

