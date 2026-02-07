#include "renderer/passes/skybox_pass.h"
#include "renderer/pipeline_builder.h"
#include "pch.h"

#include <glm/fwd.hpp>

namespace yuubi {
    SkyboxPass::SkyboxPass(const CreateInfo &createInfo) {
        const auto device = createInfo.device;

        const auto vertShader = loadShader("shaders/skybox.vert.spv", *device);
        const auto fragShader = loadShader("shaders/skybox.frag.spv", *device);

        std::vector pushConstantRanges{
            vk::PushConstantRange{
                                  .stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                                  .offset = 0,
                                  .size = sizeof(PushConstants)
            }
        };
        pipelineLayout_ = createPipelineLayout(*device, createInfo.descriptorSetLayouts, pushConstantRanges);

        PipelineBuilder builder(pipelineLayout_);

        pipeline_ = builder.setShaders(vertShader, fragShader)
                        .setInputTopology(vk::PrimitiveTopology::eTriangleList)
                        .setPolygonMode(vk::PolygonMode::eFill)
                        .setCullMode(vk::CullModeFlagBits::eFront, vk::FrontFace::eClockwise)
                        .setMultisamplingNone()
                        .disableBlending()
                        .enableDepthTest(true, vk::CompareOp::eLessOrEqual)
                        .setColorAttachmentFormats(createInfo.colorAttachmentFormats)
                        .setDepthFormat(createInfo.depthAttachmentFormat)
                        .build(*device);
    }

    SkyboxPass &SkyboxPass::operator=(SkyboxPass &&rhs) noexcept {
        if (this != &rhs) {
            std::swap(pipelineLayout_, rhs.pipelineLayout_);
            std::swap(pipeline_, rhs.pipeline_);
        }

        return *this;
    }
    void SkyboxPass::render(const RenderInfo &renderInfo) const {
        const std::array colorAttachmentInfos{
            vk::RenderingAttachmentInfo{
                                        .imageView = renderInfo.color.imageView,
                                        .imageLayout = vk::ImageLayout::eGeneral,
                                        .loadOp = vk::AttachmentLoadOp::eLoad,
                                        .storeOp = vk::AttachmentStoreOp::eStore,
                                        }
        };

        const vk::RenderingAttachmentInfo depthAttachmentInfo{
            .imageView = renderInfo.depth.imageView,
            .imageLayout = vk::ImageLayout::eGeneral,
            .loadOp = vk::AttachmentLoadOp::eLoad,
            .storeOp = vk::AttachmentStoreOp::eNone,
        };

        const vk::RenderingInfo renderingInfo{
            .renderArea = {.offset = {0, 0}, .extent = renderInfo.viewportExtent},
            .layerCount = 1,
            .colorAttachmentCount = colorAttachmentInfos.size(),
            .pColorAttachments = colorAttachmentInfos.data(),
            .pDepthAttachment = &depthAttachmentInfo
        };

        const auto &commandBuffer = renderInfo.commandBuffer;

        commandBuffer.beginRendering(renderingInfo);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline_);

        // NOTE: Viewport is flipped vertically to match OpenGL/GLM's
        // clip coordinate system where the origin is at the bottom left
        // and the y-axis points upwards.
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

        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, *pipelineLayout_, 0, {renderInfo.descriptorSets}, {}
        );

        commandBuffer.pushConstants<PushConstants>(
            *pipelineLayout_, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
            renderInfo.pushConstants
        );

        commandBuffer.draw(36, 1, 0, 0);

        commandBuffer.endRendering();
    }
} // yuubi
