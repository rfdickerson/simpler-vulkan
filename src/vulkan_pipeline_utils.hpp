#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

#include "device.hpp"

// Read binary file (e.g. SPIR-V shader) into memory.
std::vector<char> readBinaryFile(const std::string& filepath);

// RAII helper around VkShaderModule creation/destruction.
class ShaderModule {
public:
    ShaderModule(Device& device, const std::string& filepath);
    ~ShaderModule();

    ShaderModule(const ShaderModule&) = delete;
    ShaderModule& operator=(const ShaderModule&) = delete;

    ShaderModule(ShaderModule&& other) noexcept;
    ShaderModule& operator=(ShaderModule&& other) noexcept;

    VkPipelineShaderStageCreateInfo stage(VkShaderStageFlagBits stage,
                                          const char* entryPoint = "main") const;

    VkShaderModule handle() const { return module_; }

private:
    void cleanup();

    Device* device_ = nullptr;
    VkShaderModule module_ = VK_NULL_HANDLE;
};

// Utility to assemble a graphics pipeline with dynamic rendering.
class GraphicsPipelineBuilder {
public:
    GraphicsPipelineBuilder();

    GraphicsPipelineBuilder& addStage(const ShaderModule& module,
                                      VkShaderStageFlagBits stage,
                                      const char* entryPoint = "main");

    GraphicsPipelineBuilder& setVertexInput(const VkPipelineVertexInputStateCreateInfo& info);
    GraphicsPipelineBuilder& setInputAssembly(const VkPipelineInputAssemblyStateCreateInfo& info);
    GraphicsPipelineBuilder& setViewport(const VkPipelineViewportStateCreateInfo& info);
    GraphicsPipelineBuilder& setRasterization(const VkPipelineRasterizationStateCreateInfo& info);
    GraphicsPipelineBuilder& setMultisample(const VkPipelineMultisampleStateCreateInfo& info);
    GraphicsPipelineBuilder& setColorBlend(const VkPipelineColorBlendStateCreateInfo& info);
    GraphicsPipelineBuilder& setDepthStencil(const VkPipelineDepthStencilStateCreateInfo& info);
    GraphicsPipelineBuilder& clearDepthStencil();
    GraphicsPipelineBuilder& setDynamicStates(std::vector<VkDynamicState> states);
    GraphicsPipelineBuilder& setRenderingInfo(const VkPipelineRenderingCreateInfo& info);

    VkGraphicsPipelineCreateInfo createInfo(VkPipelineLayout layout) const;
    VkPipeline build(Device& device, VkPipelineLayout layout, const char* debugName) const;

private:
    VkGraphicsPipelineCreateInfo baseInfo_{};
    VkPipelineRenderingCreateInfo renderingInfo_{};
    bool hasRenderingInfo_ = false;

    VkPipelineVertexInputStateCreateInfo vertexInput_{};
    bool hasVertexInput_ = false;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly_{};
    bool hasInputAssembly_ = false;

    VkPipelineViewportStateCreateInfo viewport_{};
    bool hasViewport_ = false;

    VkPipelineRasterizationStateCreateInfo rasterization_{};
    bool hasRasterization_ = false;

    VkPipelineMultisampleStateCreateInfo multisample_{};
    bool hasMultisample_ = false;

    VkPipelineColorBlendStateCreateInfo colorBlend_{};
    bool hasColorBlend_ = false;

    VkPipelineDepthStencilStateCreateInfo depthStencil_{};
    bool hasDepthStencil_ = false;

    VkPipelineDynamicStateCreateInfo dynamicState_{};
    bool hasDynamicState_ = false;
    std::vector<VkDynamicState> dynamicStates_;

    std::vector<VkPipelineShaderStageCreateInfo> stages_;
};

// Convenience helpers for descriptor resources.
VkDescriptorSetLayout createDescriptorSetLayout(Device& device,
                                                const std::vector<VkDescriptorSetLayoutBinding>& bindings,
                                                const char* debugName);

VkDescriptorPool createDescriptorPool(Device& device,
                                      const std::vector<VkDescriptorPoolSize>& poolSizes,
                                      uint32_t maxSets,
                                      const char* debugName);

VkDescriptorSet allocateDescriptorSet(Device& device,
                                      VkDescriptorPool pool,
                                      VkDescriptorSetLayout layout,
                                      const char* debugName);

