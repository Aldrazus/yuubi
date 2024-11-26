//
// Created by aespi on 11/15/2024.
//

#include "cubemap_pass.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <renderer/device.h>
#include <renderer/pipeline_builder.h>

// Render onto a cubemap color attachment using multiview.
namespace yuubi {
    CubemapPass::CubemapPass(const CreateInfo &createInfo) {
        const auto device = createInfo.device;

        auto vertShader = loadShader("shaders/screen_quad.vert.spv", *device);
        const auto fragShader = loadShader("shaders/cubemap.frag.spv", *device);

        pipelineLayout_ = createPipelineLayout(*device, createInfo.descriptorSetLayouts, {});

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
                            .setViewMask(0b00111111)
                            .build(*device);
    }

    CubemapPass &CubemapPass::operator=(CubemapPass &&rhs) noexcept {
        if (this != &rhs) {
            std::swap(pipelineLayout_, rhs.pipelineLayout_);
            std::swap(pipeline_, rhs.pipeline_);
        }
        return *this;
    }

    void CubemapPass::render(const RenderInfo &renderInfo) const {
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
                .layerCount = 6,
                .viewMask = 0b00111111,
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

        // TODO: consider dispatching a compute shader
        commandBuffer.draw(3, 1, 0, 0);

        commandBuffer.endRendering();
    }
} // yuubi
