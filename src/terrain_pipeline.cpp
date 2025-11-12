#include "terrain_pipeline.hpp"
#include "vulkan_pipeline_utils.hpp"
#include "hex_mesh.hpp"

#include <iostream>
#include <stdexcept>
#include <array>
#include <vector>

void createTerrainPipeline(Device& device, Swapchain& swapchain, TerrainPipeline& pipeline) {
    ShaderModule vertexShader(device, "../shaders/terrain.vert.spv");
    ShaderModule fragmentShader(device, "../shaders/terrain.frag.spv");
    ShaderModule depthFragmentShader(device, "../shaders/terrain_depth.frag.spv");

    auto bindingDescription = HexMesh::getBindingDescription();
    auto attributeDescriptions = HexMesh::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor (dynamic)
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // Rasterization
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;  // Temporarily disable culling to debug
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = (swapchain.msaaSamples != VK_SAMPLE_COUNT_1_BIT)
        ? swapchain.msaaSamples
        : VK_SAMPLE_COUNT_1_BIT;

    // Blending (opaque rendering for now)
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Dynamic states
    const std::vector<VkDynamicState> dynamicStates{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    // Depth-stencil state for main pass (depth already written in prepass)
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_FALSE; // Don't write depth in main pass (prepass did it)
    depthStencil.depthCompareOp = VK_COMPARE_OP_EQUAL; // Only render fragments at prepass depth
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // Descriptor set layout (binding 0: UBO, binding 1: SSAO sampler)
    VkDescriptorSetLayoutBinding bindings[2]{};
    // UBO
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    // SSAO texture
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::vector<VkDescriptorSetLayoutBinding> layoutBindings{bindings[0], bindings[1]};
    pipeline.descriptorSetLayout = createDescriptorSetLayout(device, layoutBindings, "terrain pipeline layout");

    // Push constants
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(TerrainPushConstants);

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &pipeline.descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device.device, &pipelineLayoutInfo, nullptr, &pipeline.pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create terrain pipeline layout!");
    }

    // Dynamic rendering info
    VkFormat colorFormat = swapchain.format;
    VkPipelineRenderingCreateInfo mainRendering{};
    mainRendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    mainRendering.colorAttachmentCount = 1;
    mainRendering.pColorAttachmentFormats = &colorFormat;
    mainRendering.depthAttachmentFormat = swapchain.depthFormat;

    GraphicsPipelineBuilder mainPipelineBuilder;
    mainPipelineBuilder.addStage(vertexShader, VK_SHADER_STAGE_VERTEX_BIT)
                      .addStage(fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT)
                      .setVertexInput(vertexInputInfo)
                      .setInputAssembly(inputAssembly)
                      .setViewport(viewportState)
                      .setRasterization(rasterizer)
                      .setMultisample(multisampling)
                      .setColorBlend(colorBlending)
                      .setDepthStencil(depthStencil)
                      .setDynamicStates(dynamicStates)
                      .setRenderingInfo(mainRendering);

    pipeline.pipeline = mainPipelineBuilder.build(device, pipeline.pipelineLayout, "terrain graphics pipeline");

    VkPipelineDepthStencilStateCreateInfo depthOnlyDepthStencil{};
    depthOnlyDepthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthOnlyDepthStencil.depthTestEnable = VK_TRUE;
    depthOnlyDepthStencil.depthWriteEnable = VK_TRUE;
    depthOnlyDepthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthOnlyDepthStencil.depthBoundsTestEnable = VK_FALSE;
    depthOnlyDepthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineRenderingCreateInfo depthRendering{};
    depthRendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    depthRendering.colorAttachmentCount = 0;
    depthRendering.pColorAttachmentFormats = nullptr;
    depthRendering.depthAttachmentFormat = swapchain.depthFormat;

    GraphicsPipelineBuilder depthPipelineBuilder;
    depthPipelineBuilder.addStage(vertexShader, VK_SHADER_STAGE_VERTEX_BIT)
                        .addStage(depthFragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT)
                        .setVertexInput(vertexInputInfo)
                        .setInputAssembly(inputAssembly)
                        .setViewport(viewportState)
                        .setRasterization(rasterizer)
                        .setMultisample(multisampling)
                        .setDepthStencil(depthOnlyDepthStencil)
                        .setDynamicStates(dynamicStates)
                        .setRenderingInfo(depthRendering);

    pipeline.depthOnlyPipeline = depthPipelineBuilder.build(device, pipeline.pipelineLayout, "terrain depth-only pipeline");

    // Create uniform buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(TerrainParamsUBO);
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    VmaAllocationInfo allocInfoResult;
    if (vmaCreateBuffer(device.allocator, &bufferInfo, &allocInfo, &pipeline.uniformBuffer, 
                       &pipeline.uniformAllocation, &allocInfoResult) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create terrain uniform buffer!");
    }
    
    pipeline.uniformMapped = allocInfoResult.pMappedData;

    // Create descriptor pool
    std::vector<VkDescriptorPoolSize> poolSizesVec{
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}
    };
    pipeline.descriptorPool = createDescriptorPool(device, poolSizesVec, 1, "terrain pipeline");
    pipeline.descriptorSet = allocateDescriptorSet(device, pipeline.descriptorPool, pipeline.descriptorSetLayout, "terrain pipeline");

    // Update descriptor set (binding 0: UBO). SSAO (binding 1) updated separately.
    VkDescriptorBufferInfo bufferInfoDesc{};
    bufferInfoDesc.buffer = pipeline.uniformBuffer;
    bufferInfoDesc.offset = 0;
    bufferInfoDesc.range = sizeof(TerrainParamsUBO);

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = pipeline.descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfoDesc;

    vkUpdateDescriptorSets(device.device, 1, &descriptorWrite, 0, nullptr);

    std::cout << "Terrain pipeline created successfully." << std::endl;
}

void destroyTerrainPipeline(Device& device, TerrainPipeline& pipeline) {
    if (pipeline.commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device.device, pipeline.commandPool, nullptr);
        pipeline.commandPool = VK_NULL_HANDLE;
    }
    
    if (pipeline.uniformBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(device.allocator, pipeline.uniformBuffer, pipeline.uniformAllocation);
        pipeline.uniformBuffer = VK_NULL_HANDLE;
    }
    
    if (pipeline.descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device.device, pipeline.descriptorPool, nullptr);
        pipeline.descriptorPool = VK_NULL_HANDLE;
    }
    
    if (pipeline.descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device.device, pipeline.descriptorSetLayout, nullptr);
        pipeline.descriptorSetLayout = VK_NULL_HANDLE;
    }
    
    if (pipeline.depthOnlyPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device.device, pipeline.depthOnlyPipeline, nullptr);
        pipeline.depthOnlyPipeline = VK_NULL_HANDLE;
    }
    
    if (pipeline.pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device.device, pipeline.pipeline, nullptr);
        pipeline.pipeline = VK_NULL_HANDLE;
    }
    
    if (pipeline.pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device.device, pipeline.pipelineLayout, nullptr);
        pipeline.pipelineLayout = VK_NULL_HANDLE;
    }
}

void createTerrainCommandBuffers(Device& device, TerrainPipeline& pipeline, uint32_t count) {
    // Create command pool
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = 0;

    if (vkCreateCommandPool(device.device, &poolInfo, nullptr, &pipeline.commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create terrain command pool!");
    }

    // Allocate command buffers
    pipeline.commandBuffers.resize(count);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = pipeline.commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = count;

    if (vkAllocateCommandBuffers(device.device, &allocInfo, pipeline.commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate terrain command buffers!");
    }
}

void updateTerrainParams(TerrainPipeline& pipeline, const TerrainParamsUBO& params) {
    memcpy(pipeline.uniformMapped, &params, sizeof(TerrainParamsUBO));
}

void updateTerrainSsaoDescriptor(Device& device, TerrainPipeline& pipeline, VkImageView ssaoView, VkSampler ssaoSampler) {
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = ssaoView;
    imageInfo.sampler = ssaoSampler;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = pipeline.descriptorSet;
    write.dstBinding = 1;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device.device, 1, &write, 0, nullptr);
}

