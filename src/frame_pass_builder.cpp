#include "frame_pass_builder.hpp"

namespace FramePassBuilder {
RenderPassDesc buildDepthPrepass(const Swapchain& swapchain, TerrainExample& terrainExample) {
    RenderAttachment depthAttachment{};
    depthAttachment.extent = swapchain.extent;
    depthAttachment.samples = swapchain.msaaSamples;
    depthAttachment.depthFormat = swapchain.depthFormat;
    depthAttachment.depthView = swapchain.depthImage.view;
    depthAttachment.depthImage = swapchain.depthImage.image;
    depthAttachment.depthResolveView = swapchain.depthResolved.view;
    depthAttachment.depthResolveImage = swapchain.depthResolved.image;

    RenderPassDesc pass{};
    pass.name = "depth_prepass";
    pass.attachments = depthAttachment;
    pass.clearDepth = 1.0f;
    pass.clearStencil = 0;
    pass.depthLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    pass.record = [&terrainExample](VkCommandBuffer cmd) { terrainExample.renderDepthOnly(cmd); };
    return pass;
}

RenderPassDesc buildSsaoPass(const Swapchain& swapchain, TerrainExample& terrainExample) {
    RenderAttachment ssaoAttachment{};
    ssaoAttachment.extent = swapchain.extent;
    ssaoAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    ssaoAttachment.colorFormat = swapchain.ssaoFormat;
    ssaoAttachment.colorView = swapchain.ssaoImage.view;
    ssaoAttachment.colorImage = swapchain.ssaoImage.image;
    ssaoAttachment.depthFormat = swapchain.depthFormat;
    ssaoAttachment.depthView = swapchain.depthResolved.view;
    ssaoAttachment.depthImage = swapchain.depthResolved.image;

    RenderPassDesc pass{};
    pass.name = "ssao";
    pass.attachments = ssaoAttachment;
    pass.clearColor = {{1.0f, 0.0f, 0.0f, 0.0f}};
    pass.depthLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    pass.depthReadOnly = true;
    pass.record = [&terrainExample](VkCommandBuffer cmd) { terrainExample.renderSSAO(cmd); };
    return pass;
}

RenderPassDesc buildTerrainPass(const Swapchain& swapchain, TerrainExample& terrainExample) {
    RenderAttachment attachment{};
    attachment.extent = swapchain.extent;
    attachment.samples = swapchain.msaaSamples;
    attachment.colorFormat = swapchain.format;
    attachment.depthFormat = swapchain.depthFormat;

    if (swapchain.msaaSamples != VK_SAMPLE_COUNT_1_BIT) {
        attachment.colorView = swapchain.msaaColor.view;
        attachment.colorImage = swapchain.msaaColor.image;
        attachment.resolveView = swapchain.sceneColor.view;
        attachment.resolveImage = swapchain.sceneColor.image;
    } else {
        attachment.colorView = swapchain.sceneColor.view;
        attachment.colorImage = swapchain.sceneColor.image;
    }

    attachment.depthView = swapchain.depthImage.view;
    attachment.depthImage = swapchain.depthImage.image;

    RenderPassDesc pass{};
    pass.name = "terrain";
    pass.attachments = attachment;
    pass.clearColor = {{0.05f, 0.05f, 0.08f, 1.0f}};
    pass.clearDepth = 1.0f;
    pass.clearStencil = 0;
    pass.depthLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    pass.sampledImages.push_back(swapchain.ssaoImage.image);
    pass.record = [&terrainExample](VkCommandBuffer cmd) { terrainExample.render(cmd); };
    return pass;
}

RenderPassDesc buildTiltShiftPass(const Swapchain& swapchain, TerrainExample& terrainExample, uint32_t imageIndex) {
    RenderAttachment attachment{};
    attachment.extent = swapchain.extent;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.colorFormat = swapchain.format;
    attachment.colorView = swapchain.images[imageIndex].view;
    attachment.colorImage = swapchain.images[imageIndex].image;
    attachment.depthFormat = swapchain.depthFormat;
    attachment.depthView = swapchain.depthResolved.view;
    attachment.depthImage = swapchain.depthResolved.image;

    RenderPassDesc pass{};
    pass.name = "tiltshift";
    pass.attachments = attachment;
    pass.depthLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    pass.depthReadOnly = true;
    pass.sampledImages.push_back(swapchain.sceneColor.image);
    pass.record = [&terrainExample](VkCommandBuffer cmd) { terrainExample.renderTiltShift(cmd); };
    return pass;
}
} // namespace FramePassBuilder
