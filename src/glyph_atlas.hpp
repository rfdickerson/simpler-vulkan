#pragma once

#include "image.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <unordered_map>
#include <vector>

struct Device;
struct Buffer;

struct GlyphInfo {
    uint32_t glyphIndex;
    float uvX, uvY;          // UV coordinates in atlas (top-left)
    float uvW, uvH;          // UV width/height
    int32_t bearingX, bearingY; // Glyph bearing
    uint32_t width, height;     // Glyph dimensions in pixels
    int32_t advance;            // Horizontal advance
};

class GlyphAtlas {
public:
    GlyphAtlas(Device& device, uint32_t atlasWidth = 2048, uint32_t atlasHeight = 2048);
    ~GlyphAtlas();

    // Load a font from file
    bool loadFont(const std::string& fontPath, uint32_t pixelSize);

    // Add a glyph to the atlas by glyph index
    bool addGlyph(uint32_t glyphIndex);

    // Get glyph info (returns nullptr if glyph not in atlas)
    const GlyphInfo* getGlyphInfo(uint32_t glyphIndex) const;

    // Finalize the atlas and upload to GPU
    // Returns the staging buffer which must be destroyed by the caller after command buffer submission
    Buffer finalizeAtlas(VkCommandBuffer cmd);

    // Get the atlas image
    const Image& getAtlasImage() const { return atlasImage_; }

    // Get atlas dimensions
    uint32_t getAtlasWidth() const { return atlasWidth_; }
    uint32_t getAtlasHeight() const { return atlasHeight_; }

private:
    Device& device_;
    FT_Library ftLibrary_;
    FT_Face ftFace_;

    uint32_t atlasWidth_;
    uint32_t atlasHeight_;
    uint32_t currentX_;
    uint32_t currentY_;
    uint32_t currentRowHeight_;

    std::vector<uint8_t> atlasData_;
    std::unordered_map<uint32_t, GlyphInfo> glyphMap_;

    Image atlasImage_;
    bool finalized_;

    // Helper to find space in atlas for a glyph
    bool findSpaceForGlyph(uint32_t width, uint32_t height, uint32_t& outX, uint32_t& outY);
};

// Create a sampler for the glyph atlas
VkSampler createAtlasSampler(Device& device);

