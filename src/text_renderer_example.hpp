#pragma once

// Example usage of GlyphAtlas for text rendering with Vulkan
// 
// This file demonstrates how to use the image.hpp and glyph_atlas.hpp
// files to create a texture atlas for rendering text.

#include "firmament/device.hpp"
#include "firmament/glyph_atlas.hpp"
#include "firmament/text.hpp"

using namespace firmament;
#include <string>

/*
 * Example workflow:
 * 
 * 1. Create a GlyphAtlas instance
 *    GlyphAtlas atlas(device, 2048, 2048);
 * 
 * 2. Load a font
 *    atlas.loadFont("assets/fonts/EBGaramond-Regular.ttf", 48);
 * 
 * 3. Add glyphs you need (can be done on-demand or pre-loaded)
 *    // Option A: Pre-load common characters
 *    for (char c = 32; c < 127; ++c) {
 *        uint32_t glyphIndex = FT_Get_Char_Index(face, c);
 *        atlas.addGlyph(glyphIndex);
 *    }
 *    
 *    // Option B: Use HarfBuzz to get glyphs from shaped text
 *    HbShaper shaper("assets/fonts/EBGaramond-Regular.ttf", 48);
 *    auto shapedGlyphs = shaper.shape_utf8("Hello, World!");
 *    for (const auto& g : shapedGlyphs) {
 *        atlas.addGlyph(g.glyph_index);
 *    }
 * 
 * 4. Finalize the atlas (creates VkImage and uploads to GPU)
 *    VkCommandBuffer cmd = ...; // Your command buffer
 *    atlas.finalizeAtlas(cmd);
 * 
 * 5. Create a sampler for the atlas texture
 *    VkSampler sampler = createAtlasSampler(device);
 * 
 * 6. Use in rendering:
 *    const Image& atlasImage = atlas.getAtlasImage();
 *    // Bind atlasImage.view and sampler to your descriptor set
 *    
 *    // For each glyph to render:
 *    const GlyphInfo* info = atlas.getGlyphInfo(glyphIndex);
 *    if (info) {
 *        // Use info->uvX, uvY, uvW, uvH for texture coordinates
 *        // Use info->bearingX, bearingY for positioning
 *        // Use info->width, height for quad size
 *        // Use info->advance for cursor advancement
 *    }
 * 
 * 7. In your shader:
 *    layout(binding = 0) uniform sampler2D glyphAtlas;
 *    
 *    // Fragment shader
 *    float alpha = texture(glyphAtlas, uv).r;
 *    fragColor = vec4(textColor.rgb, textColor.a * alpha);
 */

class TextRenderer {
public:
    TextRenderer(Device& device, const std::string& fontPath, uint32_t fontSize)
        : device_(device), atlas_(device, 2048, 2048) {
        
        // Load font
        if (!atlas_.loadFont(fontPath, fontSize)) {
            throw std::runtime_error("Failed to load font: " + fontPath);
        }
        
        // Initialize HarfBuzz shaper
        shaper_ = std::make_unique<HbShaper>(fontPath, fontSize);
    }
    
    // Prepare text for rendering (shapes and ensures glyphs are in atlas)
    std::vector<ShapedGlyph> prepareText(const std::string& text) {
        auto shapedGlyphs = shaper_->shape_utf8(text);
        
        // Ensure all glyphs are in the atlas
        for (const auto& g : shapedGlyphs) {
            atlas_.addGlyph(g.glyph_index);
        }
        
        return shapedGlyphs;
    }
    
    // Finalize atlas (call after all text is prepared, before rendering)
    void finalizeAtlas(VkCommandBuffer cmd) {
        atlas_.finalizeAtlas(cmd);
        sampler_ = createAtlasSampler(device_);
    }
    
    // Get glyph info for rendering
    const GlyphInfo* getGlyphInfo(uint32_t glyphIndex) const {
        return atlas_.getGlyphInfo(glyphIndex);
    }
    
    // Get atlas resources for binding
    const Image& getAtlasImage() const { return atlas_.getAtlasImage(); }
    VkSampler getSampler() const { return sampler_; }
    
    ~TextRenderer() {
        if (sampler_ != VK_NULL_HANDLE) {
            vkDestroySampler(device_.device, sampler_, nullptr);
        }
    }
    
private:
    Device& device_;
    GlyphAtlas atlas_;
    std::unique_ptr<HbShaper> shaper_;
    VkSampler sampler_ = VK_NULL_HANDLE;
};

// Example rendering data structure
struct TextQuad {
    float x, y;           // Screen position
    float width, height;  // Quad size
    float uvX, uvY;       // UV coordinates
    float uvW, uvH;       // UV size
};

// Build quads for rendering shaped text
inline std::vector<TextQuad> buildTextQuads(
    const std::vector<ShapedGlyph>& shapedGlyphs,
    const GlyphAtlas& atlas,
    float startX, float startY) {
    
    std::vector<TextQuad> quads;
    float cursorX = startX;
    float cursorY = startY;
    
    for (const auto& sg : shapedGlyphs) {
        const GlyphInfo* info = atlas.getGlyphInfo(sg.glyph_index);
        if (!info || info->width == 0 || info->height == 0) {
            cursorX += sg.x_advance;
            cursorY += sg.y_advance;
            continue; // Skip missing or empty glyphs
        }
        
        TextQuad quad;
        quad.x = cursorX + sg.x_offset + info->bearingX;
        quad.y = cursorY + sg.y_offset - info->bearingY;
        quad.width = info->width;
        quad.height = info->height;
        quad.uvX = info->uvX;
        quad.uvY = info->uvY;
        quad.uvW = info->uvW;
        quad.uvH = info->uvH;
        
        quads.push_back(quad);
        
        cursorX += sg.x_advance;
        cursorY += sg.y_advance;
    }
    
    return quads;
}

