#pragma once

#include <vulkan/vulkan.h>
#include <functional>
#include <vector>
#include <unordered_map>
#include "device.hpp"
#include "swapchain.hpp"

namespace firmament {

struct RenderAttachment {
    VkImageView colorView{VK_NULL_HANDLE};
    VkImageView resolveView{VK_NULL_HANDLE};
    VkImageView depthView{VK_NULL_HANDLE};
    VkImageView depthResolveView{VK_NULL_HANDLE};
    VkExtent2D extent{};
    VkSampleCountFlagBits samples{VK_SAMPLE_COUNT_1_BIT};
    VkFormat colorFormat{VK_FORMAT_UNDEFINED};
    VkFormat depthFormat{VK_FORMAT_UNDEFINED};

    // Images are required for layout transitions; views alone are insufficient.
    VkImage colorImage{VK_NULL_HANDLE};
    VkImage resolveImage{VK_NULL_HANDLE};
    VkImage depthImage{VK_NULL_HANDLE};
    VkImage depthResolveImage{VK_NULL_HANDLE};
};

struct RenderPassDesc {
    const char* name;
    RenderAttachment attachments;
    VkClearColorValue clearColor{};
    float clearDepth{1.0f};
    uint32_t clearStencil{0};
    VkAttachmentLoadOp depthLoadOp{VK_ATTACHMENT_LOAD_OP_CLEAR}; // Control depth load operation
    bool depthReadOnly{false}; // If true, transition/use depth as READ_ONLY for sampling
    // Images that will be sampled in this pass (transition to SHADER_READ_ONLY_OPTIMAL)
    std::vector<VkImage> sampledImages;
    std::function<void(VkCommandBuffer)> record;
};

class RenderGraph {
public:
    void beginFrame(Device& device, Swapchain& swapchain, VkCommandBuffer cmd);
    void addPass(const RenderPassDesc& pass);
    void execute();
    void endFrame();
    void resetLayoutTracking(); // Call when swapchain is recreated

private:
    Device* device{nullptr};
    Swapchain* swapchain{nullptr};
    VkCommandBuffer cmd{VK_NULL_HANDLE};
    std::vector<RenderPassDesc> passes;

    // Track last images to avoid unnecessary undefined->optimal toggles
    VkImage lastMsaaImage{VK_NULL_HANDLE};
    VkImage lastDepthImage{VK_NULL_HANDLE};

    // Track current known layouts per image across frames
    std::unordered_map<VkImage, VkImageLayout> imageLayouts;
};



} // namespace firmament
