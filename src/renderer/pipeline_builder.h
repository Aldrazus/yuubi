#pragma once

#include "renderer/vulkan_usage.h"
#include "pch.h"

namespace yuubi {

class Device;

// Consider moving to device.h
vk::raii::ShaderModule loadShader(std::string_view filename, const Device& device);

// Consider moving to device.h
vk::raii::PipelineLayout createPipelineLayout(const Device& device, std::span<vk::DescriptorSetLayout> layouts, std::span<vk::PushConstantRange> pushConstantRanges);

class PipelineBuilder {
public:
    PipelineBuilder(const vk::raii::PipelineLayout& pipelineLayout) : pipelineLayout_(pipelineLayout) { clear(); }
    vk::raii::Pipeline build(const Device& device);
    void clear();
    PipelineBuilder& setShaders(const vk::raii::ShaderModule& vertexShader, const vk::raii::ShaderModule& fragmentShader);
    PipelineBuilder& setInputTopology(vk::PrimitiveTopology topology);
    PipelineBuilder& setPolygonMode(vk::PolygonMode mode);
    PipelineBuilder& setCullMode(vk::CullModeFlags cullMode, vk::FrontFace frontFace);
    PipelineBuilder& setMultisamplingNone();
    PipelineBuilder& disableBlending();
    PipelineBuilder& setColorAttachmentFormat(vk::Format format);
    PipelineBuilder& setDepthFormat(vk::Format format);
    PipelineBuilder& setDepthTest(bool enable);
    PipelineBuilder& setVertexInputInfo(std::span<vk::VertexInputBindingDescription> bindingDescriptions, std::span<vk::VertexInputAttributeDescription> attributeDescriptions);
    
private:
    const vk::raii::PipelineLayout& pipelineLayout_;
    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages_;
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly_;
    vk::PipelineRasterizationStateCreateInfo rasterizer_;
    vk::PipelineColorBlendAttachmentState colorBlendAttachment_;
    vk::PipelineMultisampleStateCreateInfo multisampling_;
    vk::PipelineDepthStencilStateCreateInfo depthStencil_;
    vk::PipelineRenderingCreateInfo renderInfo_;
    vk::Format colorAttachmentFormat_;
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo_;
    vk::PushConstantRange pushConstantRange_;
};

}
