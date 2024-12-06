#include "renderer/passes/depth_pass.h"

#include "renderer/device.h"
#include "renderer/pipeline_builder.h"
#include "renderer/viewport.h"
#include "renderer/push_constants.h"
#include "pch.h"

namespace yuubi {

    DepthPass::DepthPass(
            std::shared_ptr<Device> device, std::shared_ptr<Viewport> viewport,
            std::span<vk::DescriptorSetLayout> setLayouts
    ) : device_(std::move(device)), viewport_(std::move(viewport)) {
        const auto vertShader = loadShader("shaders/depth.vert.spv", *device_);
        const auto fragShader = loadShader("shaders/depth.frag.spv", *device_);

        std::vector pushConstantRanges = {
                vk::PushConstantRange{
                                      .stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                                      .offset = 0,
                                      .size = sizeof(PushConstants)
                }
        };

        pipelineLayout_ = createPipelineLayout(*device_, setLayouts, pushConstantRanges);
        PipelineBuilder builder(pipelineLayout_);

        pipeline_ = builder.setShaders(vertShader, fragShader)
                            .setInputTopology(vk::PrimitiveTopology::eTriangleList)
                            .setPolygonMode(vk::PolygonMode::eFill)
                            .setCullMode(vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise)
                            .setMultisamplingNone()
                            .disableBlending()
                            .enableDepthTest(true, vk::CompareOp::eGreaterOrEqual)
                            .setDepthFormat(viewport_->getDepthFormat())
                            .build(*device_);
    }

    DepthPass& DepthPass::operator=(DepthPass&& rhs) noexcept {
        if (this != &rhs) {
            std::swap(device_, rhs.device_);
            std::swap(viewport_, rhs.viewport_);
            std::swap(pipeline_, rhs.pipeline_);
            std::swap(pipelineLayout_, rhs.pipelineLayout_);
        }

        return *this;
    }

    void DepthPass::render(
            const vk::raii::CommandBuffer& commandBuffer, const DrawContext& context, const Buffer& sceneDataBuffer,
            std::span<vk::DescriptorSet> descriptorSets
    ) const {
        // Transition depth image to DepthAttachmentOptimal
        vk::ImageMemoryBarrier2 depthImageBarrier{
                .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
                .srcAccessMask = vk::AccessFlagBits2::eNone,
                .dstStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests,
                .dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite |
                                 vk::AccessFlagBits2::eDepthStencilAttachmentRead,
                .oldLayout = vk::ImageLayout::eUndefined,
                .newLayout = vk::ImageLayout::eDepthAttachmentOptimal,
                .image = viewport_->getDepthImage().getImage(),
                .subresourceRange{
                                  .aspectMask = vk::ImageAspectFlagBits::eDepth,
                                  .baseMipLevel = 0,
                                  .levelCount = vk::RemainingMipLevels,
                                  .baseArrayLayer = 0,
                                  .layerCount = vk::RemainingArrayLayers
                },
        };

        vk::DependencyInfo dependencyInfo{.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &depthImageBarrier};

        commandBuffer.pipelineBarrier2(dependencyInfo);

        vk::RenderingAttachmentInfo depthAttachmentInfo{
                .imageView = *viewport_->getDepthImageView(),
                .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .clearValue = {.depthStencil = {0, 0}}
        };

        vk::RenderingInfo renderInfo{
                .renderArea = {.offset = {0, 0}, .extent = viewport_->getExtent()},
                .layerCount = 1,
                .colorAttachmentCount = 0,
                .pDepthAttachment = &depthAttachmentInfo
        };

        commandBuffer.beginRendering(renderInfo);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_);

        vk::Viewport viewport{
                .x = 0.0f,
                .y = static_cast<float>(viewport_->getExtent().height),
                .width = static_cast<float>(viewport_->getExtent().width),
                .height = -static_cast<float>(viewport_->getExtent().height),
                .minDepth = 0.0f,
                .maxDepth = 1.0f
        };

        commandBuffer.setViewport(0, {viewport});

        vk::Rect2D scissor{
                .offset = {0, 0},
                  .extent = viewport_->getExtent()
        };

        commandBuffer.setScissor(0, {scissor});

        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout_, 0, {descriptorSets}, {});

        // TODO: handle transparent objects
        for (const auto& renderObject: context.opaqueSurfaces) {
            commandBuffer.bindIndexBuffer(*renderObject.indexBuffer->getBuffer(), 0, vk::IndexType::eUint32);

            commandBuffer.pushConstants<PushConstants>(
                    *pipelineLayout_, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
                    {
                            PushConstants{
                                          renderObject.transform, sceneDataBuffer.getAddress(),
                                          renderObject.vertexBuffer->getAddress(), renderObject.materialId
                            }
            }
            );

            commandBuffer.drawIndexed(static_cast<uint32_t>(renderObject.indexCount), 1, renderObject.firstIndex, 0, 0);
        }
        commandBuffer.endRendering();

        // Transition depth image to DepthReadOnlyOptimal
        depthImageBarrier = {
                .srcStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests,
                .srcAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eLateFragmentTests,
                .dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentRead,
                .oldLayout = vk::ImageLayout::eDepthAttachmentOptimal,
                .newLayout = vk::ImageLayout::eDepthReadOnlyOptimal,
                .image = viewport_->getDepthImage().getImage(),
                .subresourceRange{
                                  .aspectMask = vk::ImageAspectFlagBits::eDepth,
                                  .baseMipLevel = 0,
                                  .levelCount = vk::RemainingMipLevels,
                                  .baseArrayLayer = 0,
                                  .layerCount = vk::RemainingArrayLayers
                },
        };

        dependencyInfo = {.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &depthImageBarrier};

        commandBuffer.pipelineBarrier2(dependencyInfo);
    }

}
