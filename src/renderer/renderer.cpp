#include "renderer/renderer.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "renderer/camera.h"
#include "renderer/loaded_gltf.h"
#include "renderer/render_object.h"
#include "renderer/vma/buffer.h"
#include "renderer/vulkan_usage.h"
#include "renderer/pipeline_builder.h"
#include "renderer/descriptor_layout_builder.h"
#include "renderer/vulkan/util.h"
#include "renderer/push_constants.h"
#include "renderer/gpu_data.h"
#include "pch.h"

namespace yuubi {

Renderer::Renderer(const Window& window) : window_(window) {
    instance_ = Instance{context_};

    VkSurfaceKHR tmp;
    if (glfwCreateWindowSurface(
            static_cast<VkInstance>(*instance_.getInstance()),
            window_.getWindow(), nullptr, &tmp
        ) != VK_SUCCESS) {
        UB_ERROR("Unable to create window surface!");
    }

    surface_ = std::make_shared<vk::raii::SurfaceKHR>(instance_, tmp);
    device_ = std::make_shared<Device>(instance_, *surface_);
    viewport_ = Viewport{surface_, device_};
    textureManager_ = TextureManager(device_);
    imguiManager_ = ImguiManager{instance_, *device_, window_, viewport_};


    materialManager_ = MaterialManager(device_);

    // asset_ = GLTFAsset(*device_, textureManager_, materialManager_, "assets/monkey/monkey.glb");
    asset_ = GLTFAsset(*device_, textureManager_, materialManager_, "assets/sponza/Sponza.gltf");

    {
        vk::DeviceSize bufferSize = sizeof(SceneData);
        vk::BufferCreateInfo bufferCreateInfo{
            .size = bufferSize,
            .usage = vk::BufferUsageFlagBits::eStorageBuffer |
                     vk::BufferUsageFlagBits::eTransferDst |
                    vk::BufferUsageFlagBits::eShaderDeviceAddress
        };

        VmaAllocationCreateInfo shaderDataBufferAllocInfo {
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        };

        sceneDataBuffer_ = device_->createBuffer(
            bufferCreateInfo, shaderDataBufferAllocInfo 
        );

        SceneData data {
            .view = {},
            .proj = {},
            .viewproj = {},
            .ambientColor = {},
            .sunlightDirection = {},
            .sunlightColor = {},
            .materials = materialManager_.getBufferAddress(),
        };

        sceneDataBuffer_.upload(*device_, &data, sizeof(data), 0);
    }

    createGraphicsPipeline();
}

Renderer::~Renderer() { 
    device_->getDevice().waitIdle(); 
}

void Renderer::updateScene(const Camera& camera)
{
    drawContext_.opaqueSurfaces.clear();

    asset_.draw(glm::mat4(1.0f), drawContext_);

    SceneData data {
        .view = camera.getViewMatrix(),
        .proj = camera.getViewProjectionMatrix(), // TODO: only push proj matrix
        .viewproj = camera.getViewProjectionMatrix(),
        .ambientColor = glm::vec4(0.1f),
        .sunlightDirection = glm::vec4(0, 1, 0.f, 1.0f),
        .sunlightColor = glm::vec4(1.0f),
        .materials = materialManager_.getBufferAddress(),
    };
    sceneDataBuffer_.upload(*device_, &data, sizeof(data), 0);
}

void Renderer::draw(const Camera& camera, float averageFPS) {
    updateScene(camera);

    // TODO: move to ImguiManager somehow
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Average FPS");
    ImGui::Text("Average FPS: %d", (uint32_t)averageFPS);
    ImGui::End();

    ImGui::Render();

    viewport_.doFrame(
        [this, &camera](const Frame& frame, const SwapchainImage& image) {

            vk::CommandBufferBeginInfo beginInfo{};
            frame.commandBuffer.begin(beginInfo);

            // Transition swapchain image layout to COLOR_ATTACHMENT_OPTIMAL
            // before rendering
            transitionImage(
                frame.commandBuffer, image.image, vk::ImageLayout::eUndefined,
                vk::ImageLayout::eColorAttachmentOptimal
            );

            // Transition depth buffer layout to DEPTH_ATTACHMENT_OPTIMAL before
            // rendering
            transitionImage(
                frame.commandBuffer, *viewport_.getDepthImage().getImage(),
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eDepthAttachmentOptimal
            );

            vk::RenderingAttachmentInfo colorAttachmentInfo{
                .imageView = *image.imageView,
                .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .clearValue = {{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}}}
            };

            vk::RenderingAttachmentInfo depthAttachmentInfo{
                .imageView = *viewport_.getDepthImageView(),
                .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .clearValue = {.depthStencil = {1.0f, 0}}
            };

            vk::RenderingInfo renderInfo{
                .renderArea =
                    {
                                 .offset = {0, 0},
                                 .extent = viewport_.getExtent(),
                                 },
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &colorAttachmentInfo,
                .pDepthAttachment = &depthAttachmentInfo,
            };

            frame.commandBuffer.beginRendering(renderInfo);
            {
                frame.commandBuffer.bindPipeline(
                    vk::PipelineBindPoint::eGraphics, *graphicsPipeline_
                );

                // NOTE: Viewport is flipped vertically to match OpenGL/GLM's
                // clip coordinate system where the origin is at the bottom left
                // and the y-axis points upwards.
                vk::Viewport viewport{
                    .x = 0.0f,
                    .y = static_cast<float>(viewport_.getExtent().height),
                    .width = static_cast<float>(viewport_.getExtent().width),
                    .height = -static_cast<float>(viewport_.getExtent().height),
                    .minDepth = 0.0f,
                    .maxDepth = 1.0f
                };

                frame.commandBuffer.setViewport(0, {viewport});

                vk::Rect2D scissor{
                    .offset = {0, 0},
                    .extent = viewport_.getExtent()
                };

                frame.commandBuffer.setScissor(0, {scissor});

                frame.commandBuffer.bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics, *pipelineLayout_, 0,
                    {*textureManager_.getTextureSet()}, {}
                );

                for (const auto& renderObject : drawContext_.opaqueSurfaces) {
                    frame.commandBuffer.bindIndexBuffer(*renderObject.indexBuffer->getBuffer(), 0, vk::IndexType::eUint32);;

                    frame.commandBuffer.pushConstants<PushConstants>(
                        *pipelineLayout_, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
                        {PushConstants{renderObject.transform, sceneDataBuffer_.getAddress(), renderObject.vertexBuffer->getAddress(), renderObject.materialId}}
                    );

                    frame.commandBuffer.drawIndexed(
                        static_cast<uint32_t>(renderObject.indexCount), 1,
                        renderObject.firstIndex, 0, 0
                    );
                }

                // Draw UI.
                // TODO: move to ImguiManager somehow
                ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *frame.commandBuffer);

            }
            frame.commandBuffer.endRendering();

            // Transition swapchain image layout to PRESENT_SRC before
            // presenting
            transitionImage(
                frame.commandBuffer, image.image,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::ImageLayout::ePresentSrcKHR
            );

            frame.commandBuffer.end();

            vk::Semaphore waitSemaphores[]{*frame.imageAvailable};
            vk::PipelineStageFlags waitStages[]{
                vk::PipelineStageFlagBits::eColorAttachmentOutput
            };

            vk::Semaphore signalSemaphores[]{*frame.renderFinished};

            vk::SubmitInfo submitInfo{
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = waitSemaphores,
                .pWaitDstStageMask = waitStages,
                .commandBufferCount = 1,
                .pCommandBuffers = &*frame.commandBuffer,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = signalSemaphores
            };

            device_->getQueue().queue.submit({submitInfo}, *frame.inFlight);
        }
    );
}

void Renderer::createGraphicsPipeline() {
    auto vertShader = loadShader("shaders/mesh.vert.spv", *device_);
    auto fragShader = loadShader("shaders/mesh.frag.spv", *device_);

    std::vector<vk::PushConstantRange> pushConstantRanges = {
        vk::PushConstantRange{
                              .stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                              .offset = 0,
                              .size = sizeof(PushConstants),
                              }
    };

    std::vector<vk::DescriptorSetLayout> setLayouts = {
        *textureManager_.getTextureSetLayout()
    };

    pipelineLayout_ =
        createPipelineLayout(*device_, setLayouts, pushConstantRanges);
    PipelineBuilder builder(pipelineLayout_);
    graphicsPipeline_ =
        builder.setShaders(vertShader, fragShader)
            .setInputTopology(vk::PrimitiveTopology::eTriangleList)
            .setPolygonMode(vk::PolygonMode::eFill)
            .setCullMode(
                vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise
            )
            .setMultisamplingNone()
            .disableBlending()
            .setDepthTest(true)
            .setColorAttachmentFormat(viewport_.getSwapChainImageFormat())
            .setDepthFormat(viewport_.getDepthFormat())
            .build(*device_);
}

}
