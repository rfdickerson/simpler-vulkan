#include "render_graph.hpp"

void RenderGraph::beginFrame(Device& inDevice, Swapchain& inSwapchain, VkCommandBuffer inCmd) {
    device = &inDevice;
    swapchain = &inSwapchain;
    cmd = inCmd;
    passes.clear();
}

void RenderGraph::resetLayoutTracking() {
    imageLayouts.clear();
    lastMsaaImage = VK_NULL_HANDLE;
    lastDepthImage = VK_NULL_HANDLE;
}

void RenderGraph::addPass(const RenderPassDesc& pass) {
    passes.push_back(pass);
}

void RenderGraph::execute() {
    for (const auto& pass : passes) {
        // Prepare image layout transitions
        VkImageMemoryBarrier2 barriers[4]{};
        uint32_t barrierCount = 0;

        // Color or MSAA color target
        if (pass.attachments.colorImage != VK_NULL_HANDLE) {
            auto& b = barriers[barrierCount++];
            b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            // Choose a conservative src stage for layout-only transitions
            b.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            b.srcAccessMask = 0;
            b.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            b.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            // Use known old layout if tracked; otherwise assume UNDEFINED (first use or after recreate)
            VkImage oldImg = pass.attachments.colorImage;
            VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            auto it = imageLayouts.find(oldImg);
            if (it != imageLayouts.end()) {
                oldLayout = it->second;
            }
            b.oldLayout = oldLayout;
            b.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            // If transitioning from PRESENT, use BOTTOM_OF_PIPE as source stage
            if (oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
                b.srcStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
            }
            b.image = pass.attachments.colorImage;
            b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            b.subresourceRange.baseMipLevel = 0;
            b.subresourceRange.levelCount = 1;
            b.subresourceRange.baseArrayLayer = 0;
            b.subresourceRange.layerCount = 1;
            imageLayouts[pass.attachments.colorImage] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }

        // Resolve target (swapchain) if provided
        if (pass.attachments.resolveImage != VK_NULL_HANDLE) {
            auto& b = barriers[barrierCount++];
            b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            b.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            b.srcAccessMask = 0;
            b.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            b.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            VkImage oldImg = pass.attachments.resolveImage;
            VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            auto it = imageLayouts.find(oldImg);
            if (it != imageLayouts.end()) {
                oldLayout = it->second;
            }
            b.oldLayout = oldLayout;
            b.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            // If transitioning from PRESENT, use BOTTOM_OF_PIPE as source stage
            if (oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
                b.srcStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
            }
            b.image = pass.attachments.resolveImage;
            b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            b.subresourceRange.baseMipLevel = 0;
            b.subresourceRange.levelCount = 1;
            b.subresourceRange.baseArrayLayer = 0;
            b.subresourceRange.layerCount = 1;
            imageLayouts[pass.attachments.resolveImage] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }

        // Depth attachment
        if (pass.attachments.depthImage != VK_NULL_HANDLE) {
            auto& b = barriers[barrierCount++];
            b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            VkImage oldImg = pass.attachments.depthImage;
            VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            auto it = imageLayouts.find(oldImg);
            if (it != imageLayouts.end()) {
                oldLayout = it->second;
            }
            
            // If depth was written in a previous pass, we need to wait for those writes
            if (oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
                b.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
                b.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            } else {
                b.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
                b.srcAccessMask = 0;
            }
            
            // Determine destination access based on target usage
            if (pass.depthReadOnly) {
                // We will sample or read-only: transition to READ_ONLY optimal
                b.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
                b.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
            } else {
                // Attachment usage
                if (pass.depthLoadOp == VK_ATTACHMENT_LOAD_OP_LOAD) {
                    // Loading existing depth - need read access
                    b.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
                    b.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                } else {
                    // Clearing depth - just write access
                    b.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
                    b.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                }
            }
            
            b.oldLayout = oldLayout;
            b.newLayout = pass.depthReadOnly
                ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            b.image = pass.attachments.depthImage;
            b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            b.subresourceRange.baseMipLevel = 0;
            b.subresourceRange.levelCount = 1;
            b.subresourceRange.baseArrayLayer = 0;
            b.subresourceRange.layerCount = 1;
            lastDepthImage = pass.attachments.depthImage;
            imageLayouts[pass.attachments.depthImage] = b.newLayout;
        }

        // Depth resolve target (single-sample) if provided
        if (pass.attachments.depthResolveImage != VK_NULL_HANDLE) {
            auto& b = barriers[barrierCount++];
            b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            VkImage oldImg = pass.attachments.depthResolveImage;
            VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            auto it = imageLayouts.find(oldImg);
            if (it != imageLayouts.end()) {
                oldLayout = it->second;
            }
            // Destination is depth attachment write (resolve destination)
            b.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            b.srcAccessMask = 0;
            b.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            b.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            b.oldLayout = oldLayout;
            b.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            b.image = pass.attachments.depthResolveImage;
            b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            b.subresourceRange.baseMipLevel = 0;
            b.subresourceRange.levelCount = 1;
            b.subresourceRange.baseArrayLayer = 0;
            b.subresourceRange.layerCount = 1;
            imageLayouts[pass.attachments.depthResolveImage] = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }

        // Any sampled images requested by this pass
        for (VkImage sampled : pass.sampledImages) {
            auto& b = barriers[barrierCount++];
            b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            auto it = imageLayouts.find(sampled);
            if (it != imageLayouts.end()) {
                oldLayout = it->second;
            }
            // Transition from whatever (likely COLOR_ATTACHMENT_OPTIMAL) to SHADER_READ_ONLY
            b.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            b.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            b.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            b.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT;
            b.oldLayout = oldLayout;
            b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            b.image = sampled;
            b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // assume color; if depth, caller should use depthReadOnly
            b.subresourceRange.baseMipLevel = 0;
            b.subresourceRange.levelCount = 1;
            b.subresourceRange.baseArrayLayer = 0;
            b.subresourceRange.layerCount = 1;
            imageLayouts[sampled] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        if (barrierCount > 0) {
            VkDependencyInfo dep{};
            dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dep.imageMemoryBarrierCount = barrierCount;
            dep.pImageMemoryBarriers = barriers;
            vkCmdPipelineBarrier2(cmd, &dep);
        }

        // Build rendering attachments
        VkRenderingAttachmentInfo colorAttachment{};
        VkRenderingAttachmentInfo* pColorAttachment = nullptr;
        if (pass.attachments.colorView != VK_NULL_HANDLE) {
            colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            colorAttachment.imageView = pass.attachments.colorView;
            colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachment.loadOp = pass.colorLoadOp;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.clearValue.color = pass.clearColor;

            // Resolve if provided
            if (pass.attachments.resolveView != VK_NULL_HANDLE) {
                colorAttachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
                colorAttachment.resolveImageView = pass.attachments.resolveView;
                colorAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            }
            pColorAttachment = &colorAttachment;
        }

        // Depth attachment (only include when writing/reading as attachment)
        VkRenderingAttachmentInfo depthAttachment{};
        VkRenderingAttachmentInfo* pDepthAttachment = nullptr;
        if (pass.attachments.depthView != VK_NULL_HANDLE && !pass.depthReadOnly) {
            depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depthAttachment.imageView = pass.attachments.depthView;
            depthAttachment.imageLayout = pass.depthReadOnly
                ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthAttachment.loadOp = pass.depthLoadOp;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Preserve depth for subsequent passes
            depthAttachment.clearValue.depthStencil = { pass.clearDepth, pass.clearStencil };
            // Depth resolve to single-sample if provided
            if (pass.attachments.depthResolveView != VK_NULL_HANDLE) {
                depthAttachment.resolveMode = VK_RESOLVE_MODE_MIN_BIT;
                depthAttachment.resolveImageView = pass.attachments.depthResolveView;
                depthAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            }
            pDepthAttachment = &depthAttachment;
        }

        // Begin dynamic rendering
        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea.offset = {0, 0};
        renderingInfo.renderArea.extent = pass.attachments.extent;
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = (pColorAttachment != nullptr) ? 1 : 0;
        renderingInfo.pColorAttachments = pColorAttachment;
        renderingInfo.pDepthAttachment = pDepthAttachment;

        vkCmdBeginRendering(cmd, &renderingInfo);
        if (pass.record) {
            pass.record(cmd);
        }
        vkCmdEndRendering(cmd);
    }
}

void RenderGraph::endFrame() {
    // Transition current swapchain image to present for display
    if (!swapchain || swapchain->images.empty()) return;

    VkImage presentImage = swapchain->images[swapchain->currentImageIndex].image;
    // The image should be in COLOR_ATTACHMENT_OPTIMAL after rendering
    // (we set it in execute() after the barrier)
    VkImageLayout currentLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkImageMemoryBarrier2 presentBarrier{};
    presentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    presentBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    presentBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    presentBarrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
    presentBarrier.dstAccessMask = 0;
    presentBarrier.oldLayout = currentLayout;
    presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    presentBarrier.image = presentImage;
    presentBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    presentBarrier.subresourceRange.baseMipLevel = 0;
    presentBarrier.subresourceRange.levelCount = 1;
    presentBarrier.subresourceRange.baseArrayLayer = 0;
    presentBarrier.subresourceRange.layerCount = 1;

    VkDependencyInfo dep{};
    dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep.imageMemoryBarrierCount = 1;
    dep.pImageMemoryBarriers = &presentBarrier;
    vkCmdPipelineBarrier2(cmd, &dep);

    // Don't track PRESENT layout - after presenting, driver may reset to UNDEFINED
    // Remove from map so next frame assumes UNDEFINED (safe transition)
    imageLayouts.erase(presentImage);
}


