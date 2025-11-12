#include "text_renderer.hpp"

#include "device.hpp"
#include "swapchain.hpp"

#include <vk_mem_alloc.h>

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <unordered_set>
#include <string>

namespace {

std::vector<TextVertex> buildVertices(const GlyphAtlas& atlas,
                                      const std::vector<ShapedGlyph>& glyphs,
                                      float startX,
                                      float startY) {
    std::vector<TextVertex> vertices;
    vertices.reserve(glyphs.size() * 6);

    float cursorX = startX;
    float cursorY = startY;

    for (const auto& glyph : glyphs) {
        const GlyphInfo* info = atlas.getGlyphInfo(glyph.glyph_index);
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

void ensureVertexBuffer(TextRendererContext& ctx, size_t vertexCount) {
    const size_t requiredSize = vertexCount * sizeof(TextVertex);
    if (requiredSize <= ctx.vertexCapacity) {
        return;
    }

    destroyBuffer(*ctx.device, ctx.vertexBuffer);
    ctx.vertexBuffer = CreateVertexBuffer(*ctx.device, requiredSize);
    ctx.vertexCapacity = requiredSize;
}

} // namespace

TextRendererContext createTextRenderer(Device& device,
                                       Swapchain& swapchain,
                                       std::string_view fontPath,
                                       uint32_t fontSize) {
    std::string font(fontPath);

    auto atlas = std::make_unique<GlyphAtlas>(device);
    if (!atlas->loadFont(font, fontSize)) {
        throw std::runtime_error("TextRendererContext: failed to load font at path " + font);
    }

    TextRendererContext ctx{};
    ctx.device = &device;
    ctx.atlas = std::move(atlas);
    ctx.shaper = std::make_unique<HbShaper>(font, static_cast<int>(fontSize));

    createTextPipeline(device, swapchain, ctx.pipeline);
    ctx.atlasSampler = createAtlasSampler(device);

    return ctx;
}

void destroyTextRenderer(TextRendererContext& ctx) {
    if (ctx.device == nullptr) {
        return;
    }

    if (ctx.atlasSampler != VK_NULL_HANDLE) {
        vkDestroySampler(ctx.device->device, ctx.atlasSampler, nullptr);
        ctx.atlasSampler = VK_NULL_HANDLE;
    }

    destroyBuffer(*ctx.device, ctx.vertexBuffer);
    destroyTextPipeline(*ctx.device, ctx.pipeline);

    ctx.atlas.reset();
    ctx.shaper.reset();
    ctx.vertices.clear();
    ctx.vertexCapacity = 0;
    ctx.vertexCount = 0;
    ctx.atlasReady = false;
    ctx.verticesDirty = false;
    ctx.device = nullptr;
}

void textRendererSetText(TextRendererContext& ctx,
                         std::string_view text,
                         float startX,
                         float startY) {
    if (ctx.device == nullptr || ctx.shaper == nullptr || ctx.atlas == nullptr) {
        throw std::runtime_error("textRendererSetText: context not initialised");
    }

    auto shapedGlyphs = ctx.shaper->shape_utf8(std::string(text));

    std::unordered_set<uint32_t> missingGlyphs;
    if (!ctx.atlasReady) {
        for (const auto& glyph : shapedGlyphs) {
            ctx.atlas->addGlyph(glyph.glyph_index);
        }
    } else {
        for (const auto& glyph : shapedGlyphs) {
            if (ctx.atlas->getGlyphInfo(glyph.glyph_index) == nullptr) {
                missingGlyphs.insert(glyph.glyph_index);
            }
        }
    }

    if (!missingGlyphs.empty()) {
        std::cerr << "textRendererSetText warning: glyphs requested after atlas finalization are missing (";
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

    ctx.vertices = buildVertices(*ctx.atlas, shapedGlyphs, startX, startY);
    ctx.vertexCount = static_cast<uint32_t>(ctx.vertices.size());
    ctx.verticesDirty = true;
}

Buffer textRendererFinalizeAtlas(TextRendererContext& ctx, VkCommandBuffer cmd) {
    if (ctx.device == nullptr || ctx.atlas == nullptr) {
        throw std::runtime_error("textRendererFinalizeAtlas: context not initialised");
    }

    if (ctx.atlasReady) {
        return Buffer{VK_NULL_HANDLE, nullptr};
    }

    Buffer staging = ctx.atlas->finalizeAtlas(cmd);
    updateTextDescriptorSet(*ctx.device, ctx.pipeline, ctx.atlas->getAtlasImage().view, ctx.atlasSampler);
    ctx.atlasReady = true;
    return staging;
}

void textRendererUploadVertices(TextRendererContext& ctx) {
    if (!ctx.verticesDirty) {
        return;
    }

    const size_t requiredVertices = ctx.vertices.size();
    ensureVertexBuffer(ctx, requiredVertices);

    if (requiredVertices == 0 || ctx.vertexBuffer.buffer == VK_NULL_HANDLE) {
        ctx.vertexCount = 0;
        ctx.verticesDirty = false;
        return;
    }

    void* mapped = nullptr;
    vmaMapMemory(ctx.device->allocator, ctx.vertexBuffer.allocation, &mapped);
    std::memcpy(mapped, ctx.vertices.data(), requiredVertices * sizeof(TextVertex));
    vmaUnmapMemory(ctx.device->allocator, ctx.vertexBuffer.allocation);

    ctx.vertexCount = static_cast<uint32_t>(requiredVertices);
    ctx.verticesDirty = false;
}

void textRendererRecordDraw(TextRendererContext& ctx,
                            VkCommandBuffer cmd,
                            VkExtent2D extent,
                            const glm::vec4& color) {
    if (!ctx.atlasReady || ctx.vertexCount == 0 || ctx.vertexBuffer.buffer == VK_NULL_HANDLE) {
        return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.pipeline.pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            ctx.pipeline.pipelineLayout, 0, 1,
                            &ctx.pipeline.descriptorSet, 0, nullptr);

    TextPushConstants pushConstants{};
    pushConstants.screenSize[0] = static_cast<float>(extent.width);
    pushConstants.screenSize[1] = static_cast<float>(extent.height);
    pushConstants.textColor[0] = color.x;
    pushConstants.textColor[1] = color.y;
    pushConstants.textColor[2] = color.z;
    pushConstants.textColor[3] = color.w;

    vkCmdPushConstants(cmd, ctx.pipeline.pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(TextPushConstants), &pushConstants);

    VkBuffer vertexBuffers[] = {ctx.vertexBuffer.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
    vkCmdDraw(cmd, ctx.vertexCount, 1, 0, 0);
}

