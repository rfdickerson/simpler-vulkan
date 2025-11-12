#include "vulkan_pipeline_utils.hpp"

#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

std::vector<char> readBinaryFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open binary file: " + filepath);
    }

    const size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    return buffer;
}

ShaderModule::ShaderModule(Device& device, const std::string& filepath)
    : device_(&device) {
    auto code = readBinaryFile(filepath);

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    if (vkCreateShaderModule(device_->device, &createInfo, nullptr, &module_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module from " + filepath);
    }
}

ShaderModule::~ShaderModule() {
    cleanup();
}

ShaderModule::ShaderModule(ShaderModule&& other) noexcept {
    *this = std::move(other);
}

ShaderModule& ShaderModule::operator=(ShaderModule&& other) noexcept {
    if (this != &other) {
        cleanup();
        device_ = other.device_;
        module_ = other.module_;
        other.device_ = nullptr;
        other.module_ = VK_NULL_HANDLE;
    }
    return *this;
}

VkPipelineShaderStageCreateInfo ShaderModule::stage(VkShaderStageFlagBits stage,
                                                    const char* entryPoint) const {
    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = stage;
    stageInfo.module = module_;
    stageInfo.pName = entryPoint;
    return stageInfo;
}

void ShaderModule::cleanup() {
    if (module_ != VK_NULL_HANDLE && device_ != nullptr) {
        vkDestroyShaderModule(device_->device, module_, nullptr);
        module_ = VK_NULL_HANDLE;
    }
    device_ = nullptr;
}

GraphicsPipelineBuilder::GraphicsPipelineBuilder() {
    baseInfo_.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::addStage(const ShaderModule& module,
                                                           VkShaderStageFlagBits stage,
                                                           const char* entryPoint) {
    stages_.push_back(module.stage(stage, entryPoint));
    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::setVertexInput(
    const VkPipelineVertexInputStateCreateInfo& info) {
    vertexInput_ = info;
    vertexInput_.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    hasVertexInput_ = true;
    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::setInputAssembly(
    const VkPipelineInputAssemblyStateCreateInfo& info) {
    inputAssembly_ = info;
    inputAssembly_.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    hasInputAssembly_ = true;
    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::setViewport(
    const VkPipelineViewportStateCreateInfo& info) {
    viewport_ = info;
    viewport_.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    hasViewport_ = true;
    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::setRasterization(
    const VkPipelineRasterizationStateCreateInfo& info) {
    rasterization_ = info;
    rasterization_.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    hasRasterization_ = true;
    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::setMultisample(
    const VkPipelineMultisampleStateCreateInfo& info) {
    multisample_ = info;
    multisample_.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    hasMultisample_ = true;
    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::setColorBlend(
    const VkPipelineColorBlendStateCreateInfo& info) {
    colorBlend_ = info;
    colorBlend_.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    hasColorBlend_ = true;
    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::setDepthStencil(
    const VkPipelineDepthStencilStateCreateInfo& info) {
    depthStencil_ = info;
    depthStencil_.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    hasDepthStencil_ = true;
    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::clearDepthStencil() {
    hasDepthStencil_ = false;
    depthStencil_ = {};
    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::setDynamicStates(
    std::vector<VkDynamicState> states) {
    dynamicStates_ = std::move(states);
    if (!dynamicStates_.empty()) {
        dynamicState_.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState_.dynamicStateCount = static_cast<uint32_t>(dynamicStates_.size());
        dynamicState_.pDynamicStates = dynamicStates_.data();
        hasDynamicState_ = true;
    } else {
        dynamicState_ = {};
        hasDynamicState_ = false;
    }
    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::setRenderingInfo(
    const VkPipelineRenderingCreateInfo& info) {
    renderingInfo_ = info;
    renderingInfo_.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    hasRenderingInfo_ = true;
    return *this;
}

VkGraphicsPipelineCreateInfo GraphicsPipelineBuilder::createInfo(VkPipelineLayout layout) const {
    VkGraphicsPipelineCreateInfo info = baseInfo_;
    info.layout = layout;
    info.stageCount = static_cast<uint32_t>(stages_.size());
    info.pStages = stages_.data();
    info.pVertexInputState = hasVertexInput_ ? const_cast<VkPipelineVertexInputStateCreateInfo*>(&vertexInput_) : nullptr;
    info.pInputAssemblyState = hasInputAssembly_ ? const_cast<VkPipelineInputAssemblyStateCreateInfo*>(&inputAssembly_) : nullptr;
    info.pViewportState = hasViewport_ ? const_cast<VkPipelineViewportStateCreateInfo*>(&viewport_) : nullptr;
    info.pRasterizationState = hasRasterization_ ? const_cast<VkPipelineRasterizationStateCreateInfo*>(&rasterization_) : nullptr;
    info.pMultisampleState = hasMultisample_ ? const_cast<VkPipelineMultisampleStateCreateInfo*>(&multisample_) : nullptr;
    info.pColorBlendState = hasColorBlend_ ? const_cast<VkPipelineColorBlendStateCreateInfo*>(&colorBlend_) : nullptr;
    info.pDepthStencilState = hasDepthStencil_ ? const_cast<VkPipelineDepthStencilStateCreateInfo*>(&depthStencil_) : nullptr;
    info.pDynamicState = hasDynamicState_ ? const_cast<VkPipelineDynamicStateCreateInfo*>(&dynamicState_) : nullptr;
    info.pNext = hasRenderingInfo_ ? const_cast<VkPipelineRenderingCreateInfo*>(&renderingInfo_) : nullptr;
    return info;
}

VkPipeline GraphicsPipelineBuilder::build(Device& device,
                                          VkPipelineLayout layout,
                                          const char* debugName) const {
    VkGraphicsPipelineCreateInfo info = createInfo(layout);

    VkPipeline pipeline = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(device.device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline) != VK_SUCCESS) {
        throw std::runtime_error(std::string("Failed to create ") + debugName);
    }
    return pipeline;
}

VkDescriptorSetLayout createDescriptorSetLayout(
    Device& device,
    const std::vector<VkDescriptorSetLayoutBinding>& bindings,
    const char* debugName) {
    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = static_cast<uint32_t>(bindings.size());
    info.pBindings = bindings.data();

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    if (vkCreateDescriptorSetLayout(device.device, &info, nullptr, &layout) != VK_SUCCESS) {
        throw std::runtime_error(std::string("Failed to create descriptor set layout: ") + debugName);
    }
    return layout;
}

VkDescriptorPool createDescriptorPool(Device& device,
                                      const std::vector<VkDescriptorPoolSize>& poolSizes,
                                      uint32_t maxSets,
                                      const char* debugName) {
    VkDescriptorPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    info.pPoolSizes = poolSizes.data();
    info.maxSets = maxSets;

    VkDescriptorPool pool = VK_NULL_HANDLE;
    if (vkCreateDescriptorPool(device.device, &info, nullptr, &pool) != VK_SUCCESS) {
        throw std::runtime_error(std::string("Failed to create descriptor pool: ") + debugName);
    }
    return pool;
}

VkDescriptorSet allocateDescriptorSet(Device& device,
                                      VkDescriptorPool pool,
                                      VkDescriptorSetLayout layout,
                                      const char* debugName) {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    if (vkAllocateDescriptorSets(device.device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error(std::string("Failed to allocate descriptor set: ") + debugName);
    }
    return descriptorSet;
}

