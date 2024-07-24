#include "pipeline_builder.h"
#include "pch.h"
#include "core/io/file.h"
#include "renderer/device.h"

namespace yuubi {

vk::raii::ShaderModule loadShader(std::string_view filename,
                                  const Device& device) {
    auto shaderCode = yuubi::readFile(filename);

    return vk::raii::ShaderModule(
        device.getDevice(),
        {.codeSize = shaderCode.size(),
         .pCode = reinterpret_cast<const uint32_t*>(shaderCode.data())});
}

vk::raii::PipelineLayout createPipelineLayout(const Device& device, std::span<vk::raii::DescriptorSetLayout> layouts, std::span<vk::PushConstantRange> pushConstantRanges) {
    return vk::raii::PipelineLayout(device.getDevice(), vk::PipelineLayoutCreateInfo{
        // TODO: fix this
        .setLayoutCount = 0,
        .pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size()),
        .pPushConstantRanges = pushConstantRanges.data()
    });
}

vk::raii::Pipeline PipelineBuilder::build(const Device& device) {
    vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1,
                                                      .scissorCount = 1};

    vk::PipelineColorBlendStateCreateInfo colorBlending{
        .logicOpEnable = vk::False,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment_};

    std::vector<vk::DynamicState> dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
    };

    vk::PipelineDynamicStateCreateInfo dynamicState{
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()};

    vk::GraphicsPipelineCreateInfo pipelineInfo{
        .pNext = &renderInfo_,
        .stageCount = static_cast<uint32_t>(shaderStages_.size()),
        .pStages = shaderStages_.data(),
        .pVertexInputState = &vertexInputInfo_,
        .pInputAssemblyState = &inputAssembly_,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer_,
        .pMultisampleState = &multisampling_,
        .pDepthStencilState = &depthStencil_,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = *pipelineLayout_,
    };

    return vk::raii::Pipeline(device.getDevice(), nullptr, pipelineInfo);
};

void PipelineBuilder::clear() {
    inputAssembly_ = {};
    rasterizer_ = {};
    colorBlendAttachment_ = {};
    multisampling_ = {};
    depthStencil_ = {};
    renderInfo_ = {};
    vertexInputInfo_ = {};
    shaderStages_.clear();
}

PipelineBuilder& PipelineBuilder::setShaders(
    const vk::raii::ShaderModule& vertexShader,
    const vk::raii::ShaderModule& fragmentShader) {
    shaderStages_.clear();
    shaderStages_.push_back(vk::PipelineShaderStageCreateInfo{
        .stage = vk::ShaderStageFlagBits::eVertex,
        .module = *vertexShader,
        .pName = "main"});

    shaderStages_.push_back(vk::PipelineShaderStageCreateInfo{
        .stage = vk::ShaderStageFlagBits::eFragment,
        .module = *fragmentShader,
        .pName = "main"});

    return *this;
}

PipelineBuilder& PipelineBuilder::setInputTopology(
    vk::PrimitiveTopology topology) {
    inputAssembly_.topology = topology;
    inputAssembly_.primitiveRestartEnable = vk::False;
    return *this;
}

PipelineBuilder& PipelineBuilder::setPolygonMode(vk::PolygonMode mode) {
    rasterizer_.polygonMode = mode;
    rasterizer_.lineWidth = 1.0f;
    return *this;
}

PipelineBuilder& PipelineBuilder::setCullMode(vk::CullModeFlags cullMode,
                                              vk::FrontFace frontFace) {
    rasterizer_.cullMode = cullMode;
    rasterizer_.frontFace = frontFace;
    return *this;
}

PipelineBuilder& PipelineBuilder::setMultisamplingNone() {
    multisampling_.sampleShadingEnable = vk::False;
    multisampling_.rasterizationSamples = vk::SampleCountFlagBits::e1;
    multisampling_.minSampleShading = 1.0f;
    multisampling_.pSampleMask = nullptr;
    multisampling_.alphaToCoverageEnable = vk::False;
    multisampling_.alphaToOneEnable = vk::False;
    return *this;
}

PipelineBuilder& PipelineBuilder::disableBlending() {
    colorBlendAttachment_.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    colorBlendAttachment_.blendEnable = vk::False;
    return *this;
}

PipelineBuilder& PipelineBuilder::setColorAttachmentFormat(vk::Format format) {
    colorAttachmentFormat_ = format;
    renderInfo_.colorAttachmentCount = 1;
    renderInfo_.pColorAttachmentFormats = &colorAttachmentFormat_;
    return *this;
}

PipelineBuilder& PipelineBuilder::setDepthFormat(vk::Format format) {
    renderInfo_.depthAttachmentFormat = format;
    return *this;
}

PipelineBuilder& PipelineBuilder::disableDepthTest() {
    depthStencil_.depthTestEnable = vk::False;
    depthStencil_.depthWriteEnable = vk::False;
    depthStencil_.depthCompareOp = vk::CompareOp::eNever;
    depthStencil_.depthBoundsTestEnable = vk::False;
    depthStencil_.stencilTestEnable = vk::False;
    depthStencil_.front = {};
    depthStencil_.back = {};
    depthStencil_.minDepthBounds = 0.f;
    depthStencil_.maxDepthBounds = 1.f;
    return *this;
}

PipelineBuilder& PipelineBuilder::setVertexInputInfo(
    std::span<vk::VertexInputBindingDescription> bindingDescriptions,
    const std::span<vk::VertexInputAttributeDescription>
        attributeDescriptions) {
    vertexInputInfo_.vertexBindingDescriptionCount =
        static_cast<uint32_t>(bindingDescriptions.size());
    vertexInputInfo_.pVertexBindingDescriptions = bindingDescriptions.data();
    vertexInputInfo_.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo_.pVertexAttributeDescriptions =
        attributeDescriptions.data();
    return *this;
}

}