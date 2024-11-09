#include "renderer/passes/composite_pass.h"
#include "renderer/device.h"
#include "renderer/vma/image.h"
#include "renderer/vma/buffer.h"
#include "renderer/pipeline_builder.h"
#include "renderer/render_object.h"

namespace yuubi {

CompositePass::CompositePass(const CreateInfo& createInfo) {
    auto device = createInfo.device;

    auto vertShader = loadShader("shaders/screen_quad.vert.spv", *device);
    auto fragShader = loadShader("shaders/screen_quad.frag.spv", *device);

    pipelineLayout_ = createPipelineLayout(
        *device, createInfo.descriptorSetLayouts, createInfo.pushConstantRanges
    );

    PipelineBuilder builder(pipelineLayout_);
    pipeline_ =
        builder.setShaders(vertShader, fragShader)
            .setInputTopology(vk::PrimitiveTopology::eTriangleList)
            .setPolygonMode(vk::PolygonMode::eFill)
            .setCullMode(
                vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise
            )
            .setMultisamplingNone()
            .disableBlending()
            .disableDepthTest()
            .setColorAttachmentFormats(createInfo.colorAttachmentFormats)
            .build(*device);
}

CompositePass& CompositePass::operator=(CompositePass&& rhs) noexcept {
    if (this != &rhs) {
        std::swap(pipeline_, rhs.pipeline_);
        std::swap(pipelineLayout_, rhs.pipelineLayout_);
    }
    return *this;
}

void CompositePass::render(const RenderInfo& renderInfo) {
    std::array<vk::RenderingAttachmentInfo, 1> colorAttachmentInfos{
        vk::RenderingAttachmentInfo{
                                    .imageView = renderInfo.color.imageView,
                                    .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                                    .loadOp = vk::AttachmentLoadOp::eClear,
                                    .storeOp = vk::AttachmentStoreOp::eStore,
                                    .clearValue = {{std::array<float, 4>{0, 0, 0, 0}}}},
    };

    vk::RenderingInfo renderingInfo{
        .renderArea = {.offset = {0, 0}, .extent = renderInfo.viewportExtent},
        .layerCount = 1,
        .colorAttachmentCount = colorAttachmentInfos.size(),
        .pColorAttachments = colorAttachmentInfos.data(),
    };

    const auto& commandBuffer = renderInfo.commandBuffer;

    commandBuffer.beginRendering(renderingInfo);

    commandBuffer.bindPipeline(
        vk::PipelineBindPoint::eGraphics, *pipeline_
    );

    // NOTE: Viewport is flipped vertically to match OpenGL/GLM's
    // clip coordinate system where the origin is at the bottom left
    // and the y-axis points upwards.
    vk::Viewport viewport{
        .x = 0.0f,
        .y = static_cast<float>(renderInfo.viewportExtent.height),
        .width = static_cast<float>(renderInfo.viewportExtent.width),
        .height = -static_cast<float>(renderInfo.viewportExtent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    commandBuffer.setViewport(0, {viewport});

    vk::Rect2D scissor{
        .offset = {0, 0},
          .extent = renderInfo.viewportExtent
    };

    commandBuffer.setScissor(0, {scissor});

    commandBuffer.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, *pipelineLayout_, 0,
        {renderInfo.descriptorSets}, {}
    );

    commandBuffer.draw(3, 1, 0, 0);

    commandBuffer.endRendering();
}

}
