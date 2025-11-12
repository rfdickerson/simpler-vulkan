#pragma once

#include "image.hpp"

#include <glm/glm.hpp>

#include <cstdint>
#include <unordered_map>
#include <vector>

struct Device;
struct Buffer;

struct UiAtlasRegion {
    uint32_t id{0};
    uint32_t width{0};
    uint32_t height{0};
    float    uvX{0.0f};
    float    uvY{0.0f};
    float    uvW{0.0f};
    float    uvH{0.0f};
    glm::vec4 padding{0.0f}; // left, top, right, bottom in pixels
};

class UiAtlas {
public:
    UiAtlas(Device& device, uint32_t atlasWidth = 2048, uint32_t atlasHeight = 2048);
    ~UiAtlas();

    UiAtlas(const UiAtlas&) = delete;
    UiAtlas& operator=(const UiAtlas&) = delete;

    UiAtlasRegion getRoundedRect(uint32_t width, uint32_t height, float radius, float feather = 1.5f);
    UiAtlasRegion getRoundedRectHighlight(uint32_t width, uint32_t height, float radius,
                                          float feather, float highlightFraction);
    UiAtlasRegion getDropShadow(uint32_t width, uint32_t height, float radius,
                                float spread, float softness, float opacity);

    Buffer finalize(VkCommandBuffer cmd);

    bool   isFinalized() const { return finalized_; }
    bool   isDirty() const { return !finalized_; }
    uint32_t width() const { return atlasWidth_; }
    uint32_t height() const { return atlasHeight_; }

    const Image& getAtlasImage() const { return atlasImage_; }

    void reset(); // Clears atlas data

private:
    struct RoundedRectKey {
        uint32_t width;
        uint32_t height;
        uint32_t radius;
        uint32_t feather;

        bool operator==(const RoundedRectKey& other) const {
            return width == other.width && height == other.height &&
                   radius == other.radius && feather == other.feather;
        }
    };

    struct HighlightKey {
        uint32_t width;
        uint32_t height;
        uint32_t radius;
        uint32_t feather;
        uint32_t fraction;

        bool operator==(const HighlightKey& other) const {
            return width == other.width && height == other.height &&
                   radius == other.radius && feather == other.feather &&
                   fraction == other.fraction;
        }
    };

    struct DropShadowKey {
        uint32_t width;
        uint32_t height;
        uint32_t radius;
        uint32_t spread;
        uint32_t softness;
        uint32_t opacity;

        bool operator==(const DropShadowKey& other) const {
            return width == other.width && height == other.height &&
                   radius == other.radius && spread == other.spread &&
                   softness == other.softness && opacity == other.opacity;
        }
    };

    struct RoundedRectKeyHash {
        size_t operator()(const RoundedRectKey& key) const;
    };

    struct HighlightKeyHash {
        size_t operator()(const HighlightKey& key) const;
    };

    struct DropShadowKeyHash {
        size_t operator()(const DropShadowKey& key) const;
    };

    UiAtlasRegion addBitmap(uint32_t width, uint32_t height, const std::vector<uint8_t>& bitmap,
                            const glm::vec4& padding = glm::vec4(0.0f));
    bool          findSpace(uint32_t width, uint32_t height, uint32_t& outX, uint32_t& outY);

    UiAtlasRegion buildRoundedRectRegion(uint32_t width, uint32_t height, float radius, float feather);
    UiAtlasRegion buildRoundedRectHighlightRegion(uint32_t width, uint32_t height, float radius,
                                                  float feather, float highlightFraction);
    UiAtlasRegion buildDropShadowRegion(uint32_t width, uint32_t height, float radius,
                                        float spread, float softness, float opacity);

    Device& device_;
    uint32_t atlasWidth_;
    uint32_t atlasHeight_;

    uint32_t cursorX_{0};
    uint32_t cursorY_{0};
    uint32_t currentRowHeight_{0};

    std::vector<uint8_t> atlasData_;

    std::unordered_map<RoundedRectKey, UiAtlasRegion, RoundedRectKeyHash> roundedRectCache_;
    std::unordered_map<HighlightKey, UiAtlasRegion, HighlightKeyHash>      highlightCache_;
    std::unordered_map<DropShadowKey, UiAtlasRegion, DropShadowKeyHash>    dropShadowCache_;

    Image atlasImage_{};
    bool  finalized_{false};

    uint32_t nextId_{1};
};

VkSampler createUiAtlasSampler(Device& device);

