#include "ssao_pipeline.hpp"
#include "vulkan_pipeline_utils.hpp"

#include <stdexcept>
#include <vector>

void createSSAOPipeline(Device& device, Swapchain& swapchain, SSAOPipeline& pipeline) {
    ShaderModule vertexShader(device, "../shaders/ssao.vert.spv");
    ShaderModule fragmentShader(device, "../shaders/ssao.frag.spv");

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cbAtt{};
    cbAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
    cbAtt.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments = &cbAtt;

    const std::vector<VkDynamicState> dynamics{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_FALSE;
    ds.depthWriteEnable = VK_FALSE;

    // Descriptor set layout: binding 0 = depth texture
    VkDescriptorSetLayoutBinding depthBinding{};
    depthBinding.binding = 0;
    depthBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    depthBinding.descriptorCount = 1;
    depthBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::vector<VkDescriptorSetLayoutBinding> layoutBindings{depthBinding};
    pipeline.descriptorSetLayout = createDescriptorSetLayout(device, layoutBindings, "ssao pipeline layout");

    // Push constants
    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pcr.offset = 0;
    pcr.size = sizeof(SSAOPushConstants);

    VkPipelineLayoutCreateInfo plInfo{};
    plInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plInfo.setLayoutCount = 1;
    plInfo.pSetLayouts = &pipeline.descriptorSetLayout;
    plInfo.pushConstantRangeCount = 1;
    plInfo.pPushConstantRanges = &pcr;
    if (vkCreatePipelineLayout(device.device, &plInfo, nullptr, &pipeline.pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create SSAO pipeline layout");
    }

    // Dynamic rendering info: single-channel R8 format
    VkFormat colorFormat = swapchain.ssaoFormat;
    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &colorFormat;

    GraphicsPipelineBuilder builder;
    builder.addStage(vertexShader, VK_SHADER_STAGE_VERTEX_BIT)
           .addStage(fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT)
           .setVertexInput(vertexInput)
           .setInputAssembly(ia)
           .setViewport(vp)
           .setRasterization(rs)
           .setMultisample(ms)
           .setColorBlend(cb)
           .setDepthStencil(ds)
           .setDynamicStates(dynamics)
           .setRenderingInfo(renderingInfo);

    pipeline.pipeline = builder.build(device, pipeline.pipelineLayout, "SSAO graphics pipeline");

    // Descriptor pool and allocate set
    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}
    };
    pipeline.descriptorPool = createDescriptorPool(device, poolSizes, 1, "ssao pipeline");
    pipeline.descriptorSet = allocateDescriptorSet(device, pipeline.descriptorPool, pipeline.descriptorSetLayout, "ssao pipeline");

    // Bind depth now
    updateSSAODepthDescriptor(device, pipeline, swapchain);
}

void updateSSAODepthDescriptor(Device& device, SSAOPipeline& pipeline, Swapchain& swapchain) {
    VkDescriptorImageInfo img{};
    img.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    img.imageView = swapchain.depthResolved.view;
    img.sampler = swapchain.ssaoSampler;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = pipeline.descriptorSet;
    write.dstBinding = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &img;

    vkUpdateDescriptorSets(device.device, 1, &write, 0, nullptr);
}

void destroySSAOPipeline(Device& device, SSAOPipeline& pipeline) {
    if (pipeline.descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device.device, pipeline.descriptorPool, nullptr);
        pipeline.descriptorPool = VK_NULL_HANDLE;
    }
    if (pipeline.pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device.device, pipeline.pipeline, nullptr);
        pipeline.pipeline = VK_NULL_HANDLE;
    }
    if (pipeline.pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device.device, pipeline.pipelineLayout, nullptr);
        pipeline.pipelineLayout = VK_NULL_HANDLE;
    }
    if (pipeline.descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device.device, pipeline.descriptorSetLayout, nullptr);
        pipeline.descriptorSetLayout = VK_NULL_HANDLE;
    }
}
