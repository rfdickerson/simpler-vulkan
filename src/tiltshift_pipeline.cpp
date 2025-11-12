#include "tiltshift_pipeline.hpp"
#include "vulkan_pipeline_utils.hpp"

#include <stdexcept>
#include <vector>

void createTiltShiftPipeline(Device& device, Swapchain& swapchain, TiltShiftPipeline& pipeline) {
    ShaderModule vertexShader(device, "../shaders/tiltshift.vert.spv");
    ShaderModule fragmentShader(device, "../shaders/tiltshift.frag.spv");

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
    cbAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
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

    // Descriptor set layout: binding 0 = scene color, binding 1 = depth
    VkDescriptorSetLayoutBinding bindings[2]{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::vector<VkDescriptorSetLayoutBinding> layoutBindings{bindings[0], bindings[1]};
    pipeline.descriptorSetLayout = createDescriptorSetLayout(device, layoutBindings, "tiltshift pipeline layout");

    // Push constants
    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pcr.offset = 0;
    pcr.size = sizeof(TiltShiftPushConstants);

    VkPipelineLayoutCreateInfo plInfo{};
    plInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plInfo.setLayoutCount = 1;
    plInfo.pSetLayouts = &pipeline.descriptorSetLayout;
    plInfo.pushConstantRangeCount = 1;
    plInfo.pPushConstantRanges = &pcr;
    if (vkCreatePipelineLayout(device.device, &plInfo, nullptr, &pipeline.pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create TiltShift pipeline layout");
    }

    // Dynamic rendering info: outputs to swapchain format
    VkFormat colorFormat = swapchain.format;
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

    pipeline.pipeline = builder.build(device, pipeline.pipelineLayout, "tiltshift graphics pipeline");

    // Descriptor pool and allocate set
    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2}
    };
    pipeline.descriptorPool = createDescriptorPool(device, poolSizes, 1, "tiltshift pipeline");
    pipeline.descriptorSet = allocateDescriptorSet(device, pipeline.descriptorPool, pipeline.descriptorSetLayout, "tiltshift pipeline");

    // Bind initial descriptors
    updateTiltShiftDescriptors(device, pipeline, swapchain);
}

void updateTiltShiftDescriptors(Device& device, TiltShiftPipeline& pipeline, Swapchain& swapchain) {
    VkDescriptorImageInfo colorInfo{};
    colorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    colorInfo.imageView = swapchain.sceneColor.view;
    colorInfo.sampler = swapchain.ssaoSampler; // reuse linear clamp sampler

    VkDescriptorImageInfo depthInfo{};
    depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    depthInfo.imageView = swapchain.depthResolved.view;
    depthInfo.sampler = swapchain.ssaoSampler;

    VkWriteDescriptorSet writes[2]{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = pipeline.descriptorSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo = &colorInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = pipeline.descriptorSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &depthInfo;

    vkUpdateDescriptorSets(device.device, 2, writes, 0, nullptr);
}

void destroyTiltShiftPipeline(Device& device, TiltShiftPipeline& pipeline) {
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


