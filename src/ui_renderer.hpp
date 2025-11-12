#pragma once

#include "buffer.hpp"
#include "text_pipeline.hpp"
#include "text_renderer_example.hpp"
#include "ui_atlas.hpp"

#include <glm/glm.hpp>

#include <string>
#include <vector>

struct Device;
struct Swapchain;

namespace ui {

enum class TextAlign {
    Left,
    Center,
    Right
};

struct DropShadowStyle {
    bool enabled{true};
    glm::vec2 offset{0.0f, 12.0f};
    float spread{24.0f};
    float softness{18.0f};
    float opacity{0.65f};
};

struct PanelStyle {
    glm::vec4 fillColor{0.1f, 0.1f, 0.12f, 0.92f};
    glm::vec4 borderColor{0.36f, 0.28f, 0.17f, 1.0f};
    glm::vec4 highlightColor{0.95f, 0.85f, 0.55f, 0.22f};
    float highlightHeight{0.35f};
    float cornerRadius{18.0f};
    float borderThickness{4.0f};
    float feather{1.5f};
    DropShadowStyle shadow{};
};

struct ButtonStateStyle {
    PanelStyle panel{};
    glm::vec4 textColor{0.92f, 0.9f, 0.85f, 1.0f};
};

struct ButtonStyle {
    ButtonStateStyle normal{};
    ButtonStateStyle hover{};
    ButtonStateStyle pressed{};
    ButtonStateStyle disabled{};
    glm::vec2 padding{28.0f, 18.0f};
    float fontSize{34.0f};
};

struct LabelStyle {
    glm::vec4 color{0.92f, 0.9f, 0.85f, 1.0f};
    float fontSize{30.0f};
};

class UiRenderer {
public:
    UiRenderer(Device& device, Swapchain& swapchain,
               const std::string& fontPath, uint32_t baseFontSize);
    ~UiRenderer();

    UiRenderer(const UiRenderer&) = delete;
    UiRenderer& operator=(const UiRenderer&) = delete;

    void beginFrame(const VkExtent2D& extent);

    void drawPanel(const glm::vec2& topLeft, const glm::vec2& size,
                   const PanelStyle& style);

    void drawButton(const glm::vec2& topLeft, const glm::vec2& size,
                    const std::string& label, const ButtonStyle& style,
                    bool hovered = false, bool pressed = false, bool enabled = true);

    void drawLabel(const glm::vec2& topLeft, const std::string& text,
                   const LabelStyle& style, TextAlign align = TextAlign::Left);

    void flush(VkCommandBuffer cmd);

    // Should be called after swapchain recreation
    void onSwapchainResized(Swapchain& swapchain);

    const PanelStyle& defaultPanelStyle() const { return defaultPanelStyle_; }
    const ButtonStyle& defaultButtonStyle() const { return defaultButtonStyle_; }
    const LabelStyle& defaultLabelStyle() const { return defaultLabelStyle_; }

private:
    struct DrawCommand {
        VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
        glm::vec4       color{1.0f};
        uint32_t        firstVertex{0};
        uint32_t        vertexCount{0};
    };

    struct TextMetrics {
        float width{0.0f};
        float minY{0.0f};
        float maxY{0.0f};
    };

    void ensureAtlasesUploaded();
    void ensureVertexCapacity(size_t vertexCount);
    glm::vec2 toBottomLeft(const glm::vec2& topLeft, float height) const;

    void appendQuad(const UiAtlasRegion& region, const glm::vec2& topLeft,
                    const glm::vec2& size, const glm::vec4& color,
                    VkDescriptorSet descriptorSet);

    void appendText(const std::vector<ShapedGlyph>& glyphs,
                    const glm::vec2& baselineTopLeft,
                    float scale,
                    const glm::vec4& color);

    TextMetrics computeTextMetrics(const std::vector<ShapedGlyph>& glyphs,
                                   float scale) const;

    Device& device_;
    Swapchain& swapchain_;

    TextPipeline pipeline_{};
    UiAtlas uiAtlas_;
    TextRenderer textRenderer_;

    VkSampler uiSampler_{VK_NULL_HANDLE};
    VkDescriptorSet uiDescriptorSet_{VK_NULL_HANDLE};
    VkDescriptorSet glyphDescriptorSet_{VK_NULL_HANDLE};

    Buffer vertexBuffer_{};
    VkDeviceSize vertexBufferSize_{0};

    std::vector<TextVertex> vertices_;
    std::vector<DrawCommand> drawCommands_;

    VkExtent2D screenExtent_{};
    uint32_t baseFontSize_{};

    PanelStyle defaultPanelStyle_{};
    ButtonStyle defaultButtonStyle_{};
    LabelStyle defaultLabelStyle_{};

    bool pendingAtlasUpload_{true};
};

} // namespace ui

