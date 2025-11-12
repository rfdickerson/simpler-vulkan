#include "ui_atlas.hpp"

#include "buffer.hpp"
#include "device.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/clamp.hpp>
#include <glm/gtx/norm.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {

constexpr uint32_t packFloat(float value, float scale) {
    return static_cast<uint32_t>(std::round(value * scale));
}

inline float smoothstep(float edge0, float edge1, float x) {
    float t = glm::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline float roundedRectSdf(const glm::vec2& p, const glm::vec2& halfSize, float radius) {
    glm::vec2 q = glm::abs(p) - (halfSize - glm::vec2(radius));
    glm::vec2 maxQ = glm::max(q, glm::vec2(0.0f));
    float outside = std::sqrt(glm::dot(maxQ, maxQ));
    float inside = std::min(std::max(q.x, q.y), 0.0f);
    return outside + inside - radius;
}

std::vector<uint8_t> generateRoundedRectBitmap(uint32_t width, uint32_t height,
                                               float radius, float feather) {
    std::vector<uint8_t> data(width * height, 0);
    glm::vec2 halfSize = glm::vec2(static_cast<float>(width), static_cast<float>(height)) * 0.5f;

    const float safeRadius = std::min(radius, std::min(halfSize.x, halfSize.y) - 0.5f);
    const float aa = std::max(feather, 0.5f);

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            glm::vec2 p = glm::vec2(static_cast<float>(x) + 0.5f,
                                    static_cast<float>(y) + 0.5f) - halfSize;
            float dist = roundedRectSdf(p, halfSize, safeRadius);
            float alpha = 1.0f - smoothstep(0.0f, aa, dist);
            alpha = glm::clamp(alpha, 0.0f, 1.0f);
            data[y * width + x] = static_cast<uint8_t>(std::round(alpha * 255.0f));
        }
    }

    return data;
}

std::vector<uint8_t> generateRoundedRectHighlightBitmap(uint32_t width, uint32_t height,
                                                        float radius, float feather,
                                                        float highlightFraction) {
    std::vector<uint8_t> data(width * height, 0);
    glm::vec2 halfSize = glm::vec2(static_cast<float>(width), static_cast<float>(height)) * 0.5f;

    const float safeRadius = std::min(radius, std::min(halfSize.x, halfSize.y) - 0.5f);
    const float aa = std::max(feather, 0.5f);
    const float highlightHeight = glm::clamp(highlightFraction, 0.0f, 1.0f) * static_cast<float>(height);

    for (uint32_t y = 0; y < height; ++y) {
        float gradient = 0.0f;
        if (highlightHeight > 0.0f) {
            float factor = 1.0f - (static_cast<float>(y) / highlightHeight);
            gradient = glm::clamp(factor, 0.0f, 1.0f);
            gradient = gradient * gradient * (3.0f - 2.0f * gradient);
        }

        for (uint32_t x = 0; x < width; ++x) {
            glm::vec2 p = glm::vec2(static_cast<float>(x) + 0.5f,
                                    static_cast<float>(y) + 0.5f) - halfSize;
            float dist = roundedRectSdf(p, halfSize, safeRadius);
            float alpha = 1.0f - smoothstep(0.0f, aa, dist);
            alpha *= gradient;
            alpha = glm::clamp(alpha, 0.0f, 1.0f);
            data[y * width + x] = static_cast<uint8_t>(std::round(alpha * 255.0f));
        }
    }

    return data;
}

struct DropShadowBitmap {
    std::vector<uint8_t> data;
    uint32_t width;
    uint32_t height;
    glm::vec4 padding;
};

DropShadowBitmap generateDropShadowBitmap(uint32_t baseWidth, uint32_t baseHeight,
                                          float radius, float spread, float softness,
                                          float opacity) {
    const float pad = spread + softness * 2.0f;
    uint32_t padPixels = static_cast<uint32_t>(std::ceil(pad));
    uint32_t width = baseWidth + padPixels * 2;
    uint32_t height = baseHeight + padPixels * 2;

    glm::vec2 halfSizeBase = glm::vec2(static_cast<float>(baseWidth),
                                       static_cast<float>(baseHeight)) * 0.5f;
    glm::vec2 halfSizeImage = glm::vec2(static_cast<float>(width),
                                        static_cast<float>(height)) * 0.5f;

    std::vector<uint8_t> data(width * height, 0);

    const float safeRadius = std::min(radius, std::min(halfSizeBase.x, halfSizeBase.y) - 0.5f);
    const float falloffStart = std::max(spread, 0.0f);
    const float falloffEnd = falloffStart + std::max(softness, 1.0f);
    const float opacityClamped = glm::clamp(opacity, 0.0f, 1.0f);

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            glm::vec2 p = glm::vec2(static_cast<float>(x) + 0.5f,
                                    static_cast<float>(y) + 0.5f) - halfSizeImage;
            float dist = roundedRectSdf(p, halfSizeBase, safeRadius);
            if (dist <= 0.0f) {
                data[y * width + x] = 0;
                continue;
            }

            float sdfAlpha = 1.0f - smoothstep(falloffStart, falloffEnd, dist);
            float alpha = opacityClamped * glm::clamp(sdfAlpha, 0.0f, 1.0f);
            data[y * width + x] = static_cast<uint8_t>(std::round(alpha * 255.0f));
        }
    }

    glm::vec4 padding(static_cast<float>(padPixels),
                      static_cast<float>(padPixels),
                      static_cast<float>(padPixels),
                      static_cast<float>(padPixels));

    return {std::move(data), width, height, padding};
}

} // namespace

size_t UiAtlas::RoundedRectKeyHash::operator()(const RoundedRectKey& key) const {
    size_t h = std::hash<uint32_t>{}(key.width);
    h = h * 1315423911u + std::hash<uint32_t>{}(key.height);
    h = h * 1315423911u + std::hash<uint32_t>{}(key.radius);
    h = h * 1315423911u + std::hash<uint32_t>{}(key.feather);
    return h;
}

size_t UiAtlas::HighlightKeyHash::operator()(const HighlightKey& key) const {
    size_t h = std::hash<uint32_t>{}(key.width);
    h = h * 1315423911u + std::hash<uint32_t>{}(key.height);
    h = h * 1315423911u + std::hash<uint32_t>{}(key.radius);
    h = h * 1315423911u + std::hash<uint32_t>{}(key.feather);
    h = h * 1315423911u + std::hash<uint32_t>{}(key.fraction);
    return h;
}

size_t UiAtlas::DropShadowKeyHash::operator()(const DropShadowKey& key) const {
    size_t h = std::hash<uint32_t>{}(key.width);
    h = h * 1315423911u + std::hash<uint32_t>{}(key.height);
    h = h * 1315423911u + std::hash<uint32_t>{}(key.radius);
    h = h * 1315423911u + std::hash<uint32_t>{}(key.spread);
    h = h * 1315423911u + std::hash<uint32_t>{}(key.softness);
    h = h * 1315423911u + std::hash<uint32_t>{}(key.opacity);
    return h;
}

UiAtlas::UiAtlas(Device& device, uint32_t atlasWidth, uint32_t atlasHeight)
    : device_(device),
      atlasWidth_(atlasWidth),
      atlasHeight_(atlasHeight),
      atlasData_(atlasWidth * atlasHeight, 0) {
}

UiAtlas::~UiAtlas() {
    if (finalized_) {
        destroyImage(device_, atlasImage_);
        finalized_ = false;
    }
}

void UiAtlas::reset() {
    if (finalized_) {
        destroyImage(device_, atlasImage_);
        finalized_ = false;
    }

    std::fill(atlasData_.begin(), atlasData_.end(), 0);
    cursorX_ = cursorY_ = currentRowHeight_ = 0;
    roundedRectCache_.clear();
    highlightCache_.clear();
    dropShadowCache_.clear();
    nextId_ = 1;
}

UiAtlasRegion UiAtlas::getRoundedRect(uint32_t width, uint32_t height, float radius, float feather) {
    RoundedRectKey key{
        width,
        height,
        packFloat(radius, 100.0f),
        packFloat(feather, 100.0f)
    };

    auto it = roundedRectCache_.find(key);
    if (it != roundedRectCache_.end()) {
        return it->second;
    }

    UiAtlasRegion region = buildRoundedRectRegion(width, height, radius, feather);
    roundedRectCache_.emplace(key, region);
    return region;
}

UiAtlasRegion UiAtlas::getRoundedRectHighlight(uint32_t width, uint32_t height, float radius,
                                               float feather, float highlightFraction) {
    HighlightKey key{
        width,
        height,
        packFloat(radius, 100.0f),
        packFloat(feather, 100.0f),
        packFloat(highlightFraction, 1000.0f)
    };

    auto it = highlightCache_.find(key);
    if (it != highlightCache_.end()) {
        return it->second;
    }

    UiAtlasRegion region = buildRoundedRectHighlightRegion(width, height, radius, feather, highlightFraction);
    highlightCache_.emplace(key, region);
    return region;
}

UiAtlasRegion UiAtlas::getDropShadow(uint32_t width, uint32_t height, float radius,
                                     float spread, float softness, float opacity) {
    DropShadowKey key{
        width,
        height,
        packFloat(radius, 100.0f),
        packFloat(spread, 100.0f),
        packFloat(softness, 100.0f),
        packFloat(opacity, 100.0f)
    };

    auto it = dropShadowCache_.find(key);
    if (it != dropShadowCache_.end()) {
        return it->second;
    }

    UiAtlasRegion region = buildDropShadowRegion(width, height, radius, spread, softness, opacity);
    dropShadowCache_.emplace(key, region);
    return region;
}

UiAtlasRegion UiAtlas::addBitmap(uint32_t width, uint32_t height, const std::vector<uint8_t>& bitmap,
                                 const glm::vec4& padding) {
    if (width == 0 || height == 0) {
        throw std::runtime_error("UiAtlas::addBitmap - zero-sized bitmap");
    }
    if (bitmap.size() != static_cast<size_t>(width * height)) {
        throw std::runtime_error("UiAtlas::addBitmap - bitmap size mismatch");
    }

    uint32_t atlasX, atlasY;
    if (!findSpace(width, height, atlasX, atlasY)) {
        throw std::runtime_error("UiAtlas is full. Increase atlas dimensions.");
    }

    for (uint32_t y = 0; y < height; ++y) {
        uint32_t destRow = atlasY + y;
        uint32_t srcRow = y * width;
        uint32_t destIdx = destRow * atlasWidth_ + atlasX;
        std::copy(bitmap.begin() + srcRow, bitmap.begin() + srcRow + width, atlasData_.begin() + destIdx);
    }

    finalized_ = false;

    UiAtlasRegion region;
    region.id = nextId_++;
    region.width = width;
    region.height = height;
    region.uvX = static_cast<float>(atlasX) / static_cast<float>(atlasWidth_);
    region.uvY = static_cast<float>(atlasY) / static_cast<float>(atlasHeight_);
    region.uvW = static_cast<float>(width) / static_cast<float>(atlasWidth_);
    region.uvH = static_cast<float>(height) / static_cast<float>(atlasHeight_);
    region.padding = padding;

    return region;
}

bool UiAtlas::findSpace(uint32_t width, uint32_t height, uint32_t& outX, uint32_t& outY) {
    const uint32_t padding = 2;

    if (cursorX_ + width + padding > atlasWidth_) {
        cursorX_ = 0;
        cursorY_ += currentRowHeight_ + padding;
        currentRowHeight_ = 0;
    }

    if (cursorY_ + height + padding > atlasHeight_) {
        return false;
    }

    outX = cursorX_;
    outY = cursorY_;

    cursorX_ += width + padding;
    currentRowHeight_ = std::max(currentRowHeight_, height);

    return true;
}

UiAtlasRegion UiAtlas::buildRoundedRectRegion(uint32_t width, uint32_t height,
                                              float radius, float feather) {
    auto bitmap = generateRoundedRectBitmap(width, height, radius, feather);
    return addBitmap(width, height, bitmap);
}

UiAtlasRegion UiAtlas::buildRoundedRectHighlightRegion(uint32_t width, uint32_t height,
                                                       float radius, float feather,
                                                       float highlightFraction) {
    auto bitmap = generateRoundedRectHighlightBitmap(width, height, radius, feather, highlightFraction);
    return addBitmap(width, height, bitmap);
}

UiAtlasRegion UiAtlas::buildDropShadowRegion(uint32_t width, uint32_t height, float radius,
                                             float spread, float softness, float opacity) {
    DropShadowBitmap bitmap = generateDropShadowBitmap(width, height, radius, spread, softness, opacity);
    return addBitmap(bitmap.width, bitmap.height, bitmap.data, bitmap.padding);
}

Buffer UiAtlas::finalize(VkCommandBuffer cmd) {
    if (finalized_) {
        destroyImage(device_, atlasImage_);
        finalized_ = false;
    }

    atlasImage_ = createImage(device_, atlasWidth_, atlasHeight_,
                              VK_FORMAT_R8_UNORM,
                              VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                              VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    createImageView(device_, atlasImage_);
    Buffer staging = uploadImageData(device_, cmd, atlasImage_, atlasData_.data(), atlasData_.size());
    finalized_ = true;
    return staging;
}

VkSampler createUiAtlasSampler(Device& device) {
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
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    VkSampler sampler = VK_NULL_HANDLE;
    if (vkCreateSampler(device.device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create UI atlas sampler");
    }

    return sampler;
}

