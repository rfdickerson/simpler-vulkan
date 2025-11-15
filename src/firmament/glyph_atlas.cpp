#include "glyph_atlas.hpp"
#include "device.hpp"
#include "buffer.hpp"

#include <stdexcept>
#include <iostream>
#include <algorithm>

namespace firmament {

GlyphAtlas::GlyphAtlas(Device& device, uint32_t atlasWidth, uint32_t atlasHeight)
    : device_(device),
      ftLibrary_(nullptr),
      ftFace_(nullptr),
      atlasWidth_(atlasWidth),
      atlasHeight_(atlasHeight),
      currentX_(0),
      currentY_(0),
      currentRowHeight_(0),
      finalized_(false) {
    
    // Initialize atlas data (single channel - grayscale)
    atlasData_.resize(atlasWidth_ * atlasHeight_, 0);

    // Initialize FreeType
    if (FT_Init_FreeType(&ftLibrary_)) {
        throw std::runtime_error("Failed to initialize FreeType library");
    }
}

GlyphAtlas::~GlyphAtlas() {
    if (finalized_) {
        destroyImage(device_, atlasImage_);
    }
    
    if (ftFace_) {
        FT_Done_Face(ftFace_);
    }
    
    if (ftLibrary_) {
        FT_Done_FreeType(ftLibrary_);
    }
}

bool GlyphAtlas::loadFont(const std::string& fontPath, uint32_t pixelSize) {
    if (ftFace_) {
        FT_Done_Face(ftFace_);
        ftFace_ = nullptr;
    }

    if (FT_New_Face(ftLibrary_, fontPath.c_str(), 0, &ftFace_)) {
        std::cerr << "Failed to load font: " << fontPath << std::endl;
        return false;
    }

    // Set pixel size
    FT_Set_Pixel_Sizes(ftFace_, 0, pixelSize);

    std::cout << "Font loaded: " << fontPath << " at " << pixelSize << "px" << std::endl;
    return true;
}

bool GlyphAtlas::addGlyph(uint32_t glyphIndex) {
    // Check if glyph already exists
    if (glyphMap_.find(glyphIndex) != glyphMap_.end()) {
        return true;
    }

    if (!ftFace_) {
        std::cerr << "No font loaded!" << std::endl;
        return false;
    }

    // Load glyph
    if (FT_Load_Glyph(ftFace_, glyphIndex, FT_LOAD_RENDER)) {
        std::cerr << "Failed to load glyph: " << glyphIndex << std::endl;
        return false;
    }

    FT_GlyphSlot slot = ftFace_->glyph;
    FT_Bitmap& bitmap = slot->bitmap;

    // Find space in atlas
    uint32_t atlasX, atlasY;
    if (!findSpaceForGlyph(bitmap.width, bitmap.rows, atlasX, atlasY)) {
        std::cerr << "Not enough space in atlas for glyph: " << glyphIndex << std::endl;
        return false;
    }

    // Copy glyph bitmap to atlas
    for (uint32_t y = 0; y < bitmap.rows; ++y) {
        for (uint32_t x = 0; x < bitmap.width; ++x) {
            uint32_t atlasIdx = (atlasY + y) * atlasWidth_ + (atlasX + x);
            uint32_t bitmapIdx = y * bitmap.width + x;
            atlasData_[atlasIdx] = bitmap.buffer[bitmapIdx];
        }
    }

    // Store glyph info
    GlyphInfo info{};
    info.glyphIndex = glyphIndex;
    info.uvX = static_cast<float>(atlasX) / atlasWidth_;
    info.uvY = static_cast<float>(atlasY) / atlasHeight_;
    info.uvW = static_cast<float>(bitmap.width) / atlasWidth_;
    info.uvH = static_cast<float>(bitmap.rows) / atlasHeight_;
    info.bearingX = slot->bitmap_left;
    info.bearingY = slot->bitmap_top;
    info.width = bitmap.width;
    info.height = bitmap.rows;
    info.advance = slot->advance.x >> 6; // Convert from 26.6 fixed point to pixels

    glyphMap_[glyphIndex] = info;

    std::cout << "Added glyph " << glyphIndex 
              << " at (" << atlasX << ", " << atlasY << ") "
              << "size: " << bitmap.width << "x" << bitmap.rows << std::endl;

    return true;
}

const GlyphInfo* GlyphAtlas::getGlyphInfo(uint32_t glyphIndex) const {
    auto it = glyphMap_.find(glyphIndex);
    if (it != glyphMap_.end()) {
        return &it->second;
    }
    return nullptr;
}

Buffer GlyphAtlas::finalizeAtlas(VkCommandBuffer cmd) {
    if (finalized_) {
        std::cerr << "Atlas already finalized!" << std::endl;
        return Buffer{VK_NULL_HANDLE, nullptr};
    }

    // Create image
    atlasImage_ = createImage(device_, atlasWidth_, atlasHeight_,
                             VK_FORMAT_R8_UNORM,
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                             VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

    // Create image view
    createImageView(device_, atlasImage_);

    // Upload atlas data to GPU - returns staging buffer that must be destroyed by caller after submission
    Buffer stagingBuffer = uploadImageData(device_, cmd, atlasImage_, atlasData_.data(), atlasData_.size());

    finalized_ = true;

    std::cout << "Atlas finalized: " << glyphMap_.size() << " glyphs, "
              << atlasWidth_ << "x" << atlasHeight_ << " pixels" << std::endl;
    
    return stagingBuffer;
}

bool GlyphAtlas::findSpaceForGlyph(uint32_t width, uint32_t height, 
                                   uint32_t& outX, uint32_t& outY) {
    // Simple left-to-right, top-to-bottom packing
    const uint32_t padding = 2; // Add padding between glyphs

    // Check if we can fit in current row
    if (currentX_ + width + padding > atlasWidth_) {
        // Move to next row
        currentX_ = 0;
        currentY_ += currentRowHeight_ + padding;
        currentRowHeight_ = 0;
    }

    // Check if we have vertical space
    if (currentY_ + height + padding > atlasHeight_) {
        return false; // Atlas is full
    }

    outX = currentX_;
    outY = currentY_;

    currentX_ += width + padding;
    currentRowHeight_ = std::max(currentRowHeight_, height);

    return true;
}

VkSampler createAtlasSampler(Device& device) {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    VkSampler sampler;
    if (vkCreateSampler(device.device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture sampler!");
    }

    return sampler;
}


} // namespace firmament
