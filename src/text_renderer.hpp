#pragma once

#include "text_pipeline.hpp"
#include "glyph_atlas.hpp"
#include "text.hpp"
#include "buffer.hpp"

#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct Device;
struct Swapchain;

struct TextRendererContext {
    Device* device = nullptr;
    TextPipeline pipeline{};
    std::unique_ptr<GlyphAtlas> atlas;
    std::unique_ptr<HbShaper> shaper;
    VkSampler atlasSampler = VK_NULL_HANDLE;
    Buffer vertexBuffer{};
    size_t vertexCapacity = 0;
    uint32_t vertexCount = 0;
    std::vector<TextVertex> vertices;
    bool atlasReady = false;
    bool verticesDirty = false;
};

TextRendererContext createTextRenderer(Device& device,
                                       Swapchain& swapchain,
                                       std::string_view fontPath,
                                       uint32_t fontSize);

void destroyTextRenderer(TextRendererContext& ctx);

void textRendererSetText(TextRendererContext& ctx,
                         std::string_view text,
                         float startX,
                         float startY);

Buffer textRendererFinalizeAtlas(TextRendererContext& ctx, VkCommandBuffer cmd);

void textRendererUploadVertices(TextRendererContext& ctx);

void textRendererRecordDraw(TextRendererContext& ctx,
                            VkCommandBuffer cmd,
                            VkExtent2D extent,
                            const glm::vec4& color);

