#include "text_renderer.hpp"

#include "device.hpp"
#include "swapchain.hpp"

#include <vk_mem_alloc.h>

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <unordered_set>

TextRenderer::TextRenderer(Device& device,
                           Swapchain& swapchain,
                           const std::string& fontPath,
                           uint32_t fontSize)
    : device_(device)
    , pipeline_{}
    , atlas_(device)
    , shaper_(fontPath, static_cast<int>(fontSize)) {
    if (!atlas_.loadFont(fontPath, fontSize)) {
        throw std::runtime_error("TextRenderer: failed to load font at path " + fontPath);
    }

    createTextPipeline(device_, swapchain, pipeline_);
    atlasSampler_ = createAtlasSampler(device_);
}

TextRenderer::~TextRenderer() {
    if (atlasSampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device_.device, atlasSampler_, nullptr);
        atlasSampler_ = VK_NULL_HANDLE;
    }

    destroyBuffer(device_, vertexBuffer_);
    destroyTextPipeline(device_, pipeline_);
}

void TextRenderer::setText(const std::string& text, float startX, float startY) {
    auto shapedGlyphs = shaper_.shape_utf8(text);

    std::unordered_set<uint32_t> missingGlyphs;
    if (!atlasReady_) {
        for (const auto& glyph : shapedGlyphs) {
            atlas_.addGlyph(glyph.glyph_index);
        }
    } else {
        for (const auto& glyph : shapedGlyphs) {
            if (atlas_.getGlyphInfo(glyph.glyph_index) == nullptr) {
                missingGlyphs.insert(glyph.glyph_index);
            }
        }
    }

    if (!missingGlyphs.empty()) {
        std::cerr << "TextRenderer warning: glyphs requested after atlas finalization are missing (";
        bool first = true;
        for (uint32_t glyphIndex : missingGlyphs) {
            if (!first) {
                std::cerr << ", ";
            }
            std::cerr << glyphIndex;
            first = false;
        }
        std::cerr << "). Consider finalizing the atlas after preparing all required texts." << std::endl;
    }

    vertices_ = buildVertices(shapedGlyphs, startX, startY);
    vertexCount_ = static_cast<uint32_t>(vertices_.size());
    verticesDirty_ = true;
}

Buffer TextRenderer::finalizeAtlas(VkCommandBuffer cmd) {
    if (atlasReady_) {
        return Buffer{VK_NULL_HANDLE, nullptr};
    }

    Buffer staging = atlas_.finalizeAtlas(cmd);
    updateTextDescriptorSet(device_, pipeline_, atlas_.getAtlasImage().view, atlasSampler_);
    atlasReady_ = true;
    return staging;
}

void TextRenderer::uploadVertexData() {
    if (!verticesDirty_) {
        return;
    }

    const size_t requiredVertices = vertices_.size();
    ensureVertexBuffer(requiredVertices);

    if (requiredVertices == 0 || vertexBuffer_.buffer == VK_NULL_HANDLE) {
        vertexCount_ = 0;
        verticesDirty_ = false;
        return;
    }

    void* mapped = nullptr;
    vmaMapMemory(device_.allocator, vertexBuffer_.allocation, &mapped);
    std::memcpy(mapped, vertices_.data(), requiredVertices * sizeof(TextVertex));
    vmaUnmapMemory(device_.allocator, vertexBuffer_.allocation);

    vertexCount_ = static_cast<uint32_t>(requiredVertices);
    verticesDirty_ = false;
}

void TextRenderer::recordDraw(VkCommandBuffer cmd, VkExtent2D extent, const glm::vec4& color) {
    if (!atlasReady_ || vertexCount_ == 0 || vertexBuffer_.buffer == VK_NULL_HANDLE) {
        return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_.pipelineLayout, 0, 1,
                            &pipeline_.descriptorSet, 0, nullptr);

    TextPushConstants pushConstants{};
    pushConstants.screenSize[0] = static_cast<float>(extent.width);
    pushConstants.screenSize[1] = static_cast<float>(extent.height);
    pushConstants.textColor[0] = color.x;
    pushConstants.textColor[1] = color.y;
    pushConstants.textColor[2] = color.z;
    pushConstants.textColor[3] = color.w;

    vkCmdPushConstants(cmd, pipeline_.pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(TextPushConstants), &pushConstants);

    VkBuffer vertexBuffers[] = {vertexBuffer_.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
    vkCmdDraw(cmd, vertexCount_, 1, 0, 0);
}

std::vector<TextVertex> TextRenderer::buildVertices(const std::vector<ShapedGlyph>& glyphs,
                                                    float startX,
                                                    float startY) const {
    std::vector<TextVertex> vertices;
    vertices.reserve(glyphs.size() * 6);

    float cursorX = startX;
    float cursorY = startY;

    for (const auto& glyph : glyphs) {
        const GlyphInfo* info = atlas_.getGlyphInfo(glyph.glyph_index);
        if (info == nullptr || info->width == 0 || info->height == 0) {
            cursorX += glyph.x_advance;
            cursorY += glyph.y_advance;
            continue;
        }

        const float x = cursorX + glyph.x_offset + static_cast<float>(info->bearingX);
        const float y = cursorY + glyph.y_offset - static_cast<float>(info->bearingY);
        const float w = static_cast<float>(info->width);
        const float h = static_cast<float>(info->height);

        const float u0 = info->uvX;
        const float v0 = info->uvY;
        const float u1 = info->uvX + info->uvW;
        const float v1 = info->uvY + info->uvH;

        vertices.push_back({{x, y}, {u0, v0}});
        vertices.push_back({{x, y + h}, {u0, v1}});
        vertices.push_back({{x + w, y}, {u1, v0}});

        vertices.push_back({{x + w, y}, {u1, v0}});
        vertices.push_back({{x, y + h}, {u0, v1}});
        vertices.push_back({{x + w, y + h}, {u1, v1}});

        cursorX += glyph.x_advance;
        cursorY += glyph.y_advance;
    }

    return vertices;
}

void TextRenderer::ensureVertexBuffer(size_t vertexCount) {
    const size_t requiredSize = vertexCount * sizeof(TextVertex);
    if (requiredSize <= vertexCapacity_) {
        return;
    }

    destroyBuffer(device_, vertexBuffer_);
    vertexBuffer_ = CreateVertexBuffer(device_, requiredSize);
    vertexCapacity_ = requiredSize;
}

