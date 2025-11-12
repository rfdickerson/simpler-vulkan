#pragma once

#include "text_pipeline.hpp"
#include "glyph_atlas.hpp"
#include "text.hpp"
#include "buffer.hpp"

#include <glm/glm.hpp>

#include <string>
#include <vector>

struct Device;
struct Swapchain;

// High-level helper that owns glyph atlas resources, GPU buffers,
// and command recording needed to render screen-space text.
class TextRenderer {
public:
    TextRenderer(Device& device,
                 Swapchain& swapchain,
                 const std::string& fontPath,
                 uint32_t fontSize);
    ~TextRenderer();

    // Shapes the supplied UTF-8 text and builds the CPU-side vertex list.
    // If the atlas is not finalized yet, required glyphs are added automatically.
    void setText(const std::string& text, float startX, float startY);

    // Finalizes and uploads the atlas image to the GPU.
    // Returns the staging buffer used for the upload; destroy it after GPU submission.
    Buffer finalizeAtlas(VkCommandBuffer cmd);

    // Uploads the CPU vertex cache into a GPU vertex buffer (host-visible).
    void uploadVertexData();

    // Records draw commands for the currently prepared text.
    // Automatically binds pipeline, descriptor set, vertex buffer, and push constants.
    void recordDraw(VkCommandBuffer cmd, VkExtent2D extent, const glm::vec4& color);

    const std::vector<TextVertex>& cpuVertices() const { return vertices_; }
    uint32_t vertexCount() const { return vertexCount_; }
    bool isAtlasReady() const { return atlasReady_; }

private:
    std::vector<TextVertex> buildVertices(const std::vector<ShapedGlyph>& glyphs,
                                          float startX,
                                          float startY) const;
    void ensureVertexBuffer(size_t vertexCount);

    Device& device_;
    TextPipeline pipeline_;
    GlyphAtlas atlas_;
    HbShaper shaper_;
    VkSampler atlasSampler_ = VK_NULL_HANDLE;
    Buffer vertexBuffer_{};
    size_t vertexCapacity_ = 0;
    uint32_t vertexCount_ = 0;
    std::vector<TextVertex> vertices_;
    bool atlasReady_ = false;
    bool verticesDirty_ = false;
};

