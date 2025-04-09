#include "renderer/passes/lighting_pass.h"
#include "renderer/device.h"
#include "renderer/vma/image.h"
#include "renderer/vma/buffer.h"
#include "renderer/pipeline_builder.h"
#include "renderer/render_object.h"

namespace yuubi {

    // TODO: only render to normals in opaque pass.
    LightingPass::LightingPass(const CreateInfo& createInfo) {
        auto device = createInfo.device;

        auto vertShader = loadShader("shaders/mesh.vert.spv", *device);
        auto fragShader = loadShader("shaders/mesh.frag.spv", *device);

        pipelineLayout_ = createPipelineLayout(*device, createInfo.descriptorSetLayouts, createInfo.pushConstantRanges);

        PipelineBuilder builder(pipelineLayout_);
        opaquePipeline_ = builder.setShaders(vertShader, fragShader)
                              .setInputTopology(vk::PrimitiveTopology::eTriangleList)
                              .setPolygonMode(vk::PolygonMode::eFill)
                              .setCullMode(vk::CullModeFlagBits::eFront, vk::FrontFace::eClockwise)
                              .setMultisamplingNone()
                              .disableBlending()
                              .enableDepthTest(false, vk::CompareOp::eEqual)
                              .setColorAttachmentFormats(createInfo.colorAttachmentFormats)
                              .setDepthFormat(createInfo.depthFormat)
                              .build(*device);

        transparentPipeline_ =
            builder.enableBlendingAlphaBlend().enableDepthTest(false, vk::CompareOp::eGreaterOrEqual).build(*device);
    }

    LightingPass& LightingPass::operator=(LightingPass&& rhs) noexcept {
        if (this != &rhs) {
            std::swap(opaquePipeline_, rhs.opaquePipeline_);
            std::swap(transparentPipeline_, rhs.transparentPipeline_);
            std::swap(pipelineLayout_, rhs.pipelineLayout_);
        }
        return *this;
    }

    void LightingPass::render(const RenderInfo& renderInfo) {
        std::array<vk::RenderingAttachmentInfo, 2> colorAttachmentInfos{
            vk::RenderingAttachmentInfo{
                                        .imageView = renderInfo.color.imageView,
                                        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                                        .loadOp = vk::AttachmentLoadOp::eClear,
                                        .storeOp = vk::AttachmentStoreOp::eStore,
                                        .clearValue = {{std::array<float, 4>{0, 0, 0, 0}}}},
            vk::RenderingAttachmentInfo{
                                        .imageView = renderInfo.normal.imageView,
                                        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                                        .loadOp = vk::AttachmentLoadOp::eClear,
                                        .storeOp = vk::AttachmentStoreOp::eStore,
                                        .clearValue = {{std::array<float, 4>{0, 0, 0, 0}}}}
        };

        vk::RenderingAttachmentInfo depthAttachmentInfo{
            .imageView = renderInfo.depth.imageView,
            .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eLoad
        };

        vk::RenderingInfo renderingInfo{
            .renderArea = {.offset = {0, 0}, .extent = renderInfo.viewportExtent},
            .layerCount = 1,
            .colorAttachmentCount = colorAttachmentInfos.size(),
            .pColorAttachments = colorAttachmentInfos.data(),
            .pDepthAttachment = &depthAttachmentInfo
        };

        const auto& commandBuffer = renderInfo.commandBuffer;

        commandBuffer.beginRendering(renderingInfo);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *opaquePipeline_);

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
            vk::PipelineBindPoint::eGraphics, *pipelineLayout_, 0, {renderInfo.descriptorSets}, {}
        );

        for (const auto& renderObject: renderInfo.context.opaqueSurfaces) {
            commandBuffer.bindIndexBuffer(*renderObject.indexBuffer->getBuffer(), 0, vk::IndexType::eUint32);

            commandBuffer.pushConstants<PushConstants>(
                *pipelineLayout_, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
                {
                    PushConstants{
                                  renderObject.transform, renderInfo.sceneDataBuffer.getAddress(),
                                  renderObject.vertexBuffer->getAddress(), renderObject.materialId
                    }
            }
            );

            commandBuffer.drawIndexed(static_cast<uint32_t>(renderObject.indexCount), 1, renderObject.firstIndex, 0, 0);
        }

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *transparentPipeline_);

        // TODO: Sort surfaces for correct output
        for (const auto& renderObject: renderInfo.context.transparentSurfaces) {
            commandBuffer.bindIndexBuffer(*renderObject.indexBuffer->getBuffer(), 0, vk::IndexType::eUint32);

            commandBuffer.pushConstants<PushConstants>(
                *pipelineLayout_, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
                {
                    PushConstants{
                                  renderObject.transform, renderInfo.sceneDataBuffer.getAddress(),
                                  renderObject.vertexBuffer->getAddress(), renderObject.materialId
                    }
            }
            );

            commandBuffer.drawIndexed(static_cast<uint32_t>(renderObject.indexCount), 1, renderObject.firstIndex, 0, 0);
        }

        commandBuffer.endRendering();
    }

}
