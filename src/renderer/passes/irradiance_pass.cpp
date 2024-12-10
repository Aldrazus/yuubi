#include "renderer/passes/irradiance_pass.h"
#include "renderer/device.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "renderer/pipeline_builder.h"

namespace yuubi {
    IrradiancePass::IrradiancePass(const CreateInfo &createInfo) {
        const auto device = createInfo.device;

        const auto vertShader = loadShader("shaders/cubemap.vert.spv", *device);
        const auto fragShader = loadShader("shaders/irradiance.frag.spv", *device);

        std::vector pushConstantRanges{
                vk::PushConstantRange{
                                      .stageFlags = vk::ShaderStageFlagBits::eVertex, .offset = 0, .size = sizeof(PushConstants)
                }
        };

        pipelineLayout_ = createPipelineLayout(*device, createInfo.descriptorSetLayouts, pushConstantRanges);

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

        viewProjectionMatricesBuffer_ = device->createBuffer(
                vk::BufferCreateInfo{
                        .size = sizeof(glm::mat4) * 6,
                        .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst |
                                 vk::BufferUsageFlagBits::eShaderDeviceAddress
                },
                vma::AllocationCreateInfo{.usage = vma::MemoryUsage::eGpuOnly}
        );

        const glm::mat4 projectionMatrix = glm::perspective(glm::radians(90.0F), 1.0F, 1000.0F, 0.001F);
        const std::array projectionViewMatrices{
                projectionMatrix *
                        glm::lookAt(
                                glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)
                        ),
                projectionMatrix *
                        glm::lookAt(
                                glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)
                        ),
                // TODO: Are the Y textures flipped on the cubemap because Y is flipped in the viewport?
                projectionMatrix *
                        glm::lookAt(
                                glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)
                        ),
                projectionMatrix *
                        glm::lookAt(
                                glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)
                        ),
                projectionMatrix *
                        glm::lookAt(
                                glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)
                        ),
                projectionMatrix *
                        glm::lookAt(
                                glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f)
                        )
        };
        viewProjectionMatricesBuffer_.upload(*device, projectionViewMatrices.data(), sizeof(projectionViewMatrices), 0);
    }

    IrradiancePass &IrradiancePass::operator=(IrradiancePass &&rhs) noexcept {
        if (this != &rhs) {
            std::swap(pipelineLayout_, rhs.pipelineLayout_);
            std::swap(pipeline_, rhs.pipeline_);
            std::swap(viewProjectionMatricesBuffer_, rhs.viewProjectionMatricesBuffer_);
        }
        return *this;
    }
    void IrradiancePass::render(const RenderInfo &renderInfo) const {
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

        commandBuffer.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics, pipelineLayout_, 0, {renderInfo.descriptorSets}, {}
        );

        commandBuffer.pushConstants<PushConstants>(
                *pipelineLayout_, vk::ShaderStageFlagBits::eVertex, 0,
                PushConstants{.viewProjectionMatrices = viewProjectionMatricesBuffer_.getAddress()}
        );

        commandBuffer.draw(36, 1, 0, 0);

        commandBuffer.endRendering();
    }
} // yuubi
