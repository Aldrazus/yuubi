#include "renderer/passes/brdflut_pass.h"
#include "renderer/device.h"
#include "renderer/pipeline_builder.h"

namespace yuubi {
    BRDFLUTPass::BRDFLUTPass(const CreateInfo &createInfo) {
        const auto device = createInfo.device;

        const auto vertShader = loadShader("shaders/screen_quad.vert.spv", *device);
        const auto fragShader = loadShader("shaders/brdflut.frag.spv", *device);

        pipelineLayout_ = createPipelineLayout(*device, {}, {});

        PipelineBuilder builder(pipelineLayout_);

        std::vector colorAttachmentFormats{createInfo.colorAttachmentFormat};
        pipeline_ = builder.setShaders(vertShader, fragShader)
                        .setInputTopology(vk::PrimitiveTopology::eTriangleList)
                        .setPolygonMode(vk::PolygonMode::eFill)
                        .setCullMode(vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise)
                        .setMultisamplingNone()
                        .disableBlending()
                        .disableDepthTest()
                        .setColorAttachmentFormats(colorAttachmentFormats)
                        .build(*device);
    }

    BRDFLUTPass &BRDFLUTPass::operator=(BRDFLUTPass &&rhs) noexcept {
        if (this != &rhs) {
            std::swap(pipelineLayout_, rhs.pipelineLayout_);
            std::swap(pipeline_, rhs.pipeline_);
        }
        return *this;
    }
    void BRDFLUTPass::render(const RenderInfo &renderInfo) const {
        const std::array colorAttachmentInfos{
            vk::RenderingAttachmentInfo{
                                        .imageView = renderInfo.color.imageView,
                                        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                                        .loadOp = vk::AttachmentLoadOp::eClear,
                                        .storeOp = vk::AttachmentStoreOp::eStore,
                                        .clearValue = {{std::array<float, 4>{0, 0, 0, 0}}}
            }
        };
        const vk::RenderingInfo renderingInfo{
            .renderArea = {.offset = {0, 0}, .extent = renderInfo.viewportExtent},
            .layerCount = 1,
            .colorAttachmentCount = colorAttachmentInfos.size(),
            .pColorAttachments = colorAttachmentInfos.data(),
        };

        const auto &commandBuffer = renderInfo.commandBuffer;

        commandBuffer.beginRendering(renderingInfo);
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline_);

        const vk::Viewport viewport{
            .x = 0.0f,
            .y = static_cast<float>(renderInfo.viewportExtent.height),
            .width = static_cast<float>(renderInfo.viewportExtent.width),
            .height = -static_cast<float>(renderInfo.viewportExtent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };
        commandBuffer.setViewport(0, {viewport});

        const vk::Rect2D scissor{
            .offset = {0, 0},
              .extent = renderInfo.viewportExtent
        };

        commandBuffer.setScissor(0, {scissor});

        commandBuffer.draw(3, 1, 0, 0);

        commandBuffer.endRendering();
    }
}
