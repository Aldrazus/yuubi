#include "renderer/renderer.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_raii.hpp>
#include "core/io/image.h"

#include "core/io/file.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "renderer/camera.h"
#include "renderer/loaded_gltf.h"
#include "renderer/vma/buffer.h"
#include "renderer/vulkan_usage.h"
#include "renderer/pipeline_builder.h"
#include "renderer/descriptor_layout_builder.h"
#include "renderer/bindless_set_manager.h"
#include "renderer/vulkan/util.h"
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
    bindlessSetManager_ = BindlessSetManager(device_);

    auto meshes = loadGltfMeshes(*device_, "assets/sponza/Sponza.gltf").value();
    UB_INFO("Number of meshes: {}", meshes.size());
    mesh_ = meshes[0];
    texture_ = Texture{*device_, "textures/texture.jpg"};
    bindlessSetManager_.addImage(texture_);
    createGraphicsPipeline();
    initImGui();
}

Renderer::~Renderer() { 
    device_->getDevice().waitIdle(); 
    ImGui_ImplVulkan_Shutdown();

}

void Renderer::draw(const Camera& camera) {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::ShowDemoWindow();

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

                frame.commandBuffer.bindVertexBuffers(
                    0, {*mesh_->vertexBuffer().getBuffer()}, {0}
                );

                frame.commandBuffer.bindIndexBuffer(
                    *mesh_->indexBuffer().getBuffer(), 0, vk::IndexType::eUint32
                );

                frame.commandBuffer.pushConstants<PushConstants>(
                    *pipelineLayout_, vk::ShaderStageFlagBits::eVertex, 0,
                    {PushConstants{camera.getViewProjectionMatrix()}}
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
                    {*bindlessSetManager_.getDescriptorSet()}, {}
                );

                for (const auto& surface : mesh_->surfaces()) {
                    frame.commandBuffer.drawIndexed(
                        static_cast<uint32_t>(surface.count), 1,
                        surface.startIndex, 0, 0
                    );
                }

                // Draw UI.
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
    auto vertShader = loadShader("shaders/shader.vert.spv", *device_);
    auto fragShader = loadShader("shaders/shader.frag.spv", *device_);

    std::vector<vk::PushConstantRange> pushConstantRanges = {
        vk::PushConstantRange{
                              .stageFlags = vk::ShaderStageFlagBits::eVertex,
                              .offset = 0,
                              .size = sizeof(PushConstants),
                              }
    };

    std::vector<vk::DescriptorSetLayout> setLayouts = {
        *bindlessSetManager_.getDescriptorSetLayout()
    };

    pipelineLayout_ =
        createPipelineLayout(*device_, setLayouts, pushConstantRanges);
    PipelineBuilder builder(pipelineLayout_);
    std::array<vk::VertexInputBindingDescription, 1> bindingDescriptions{
        Vertex::getBindingDescription()
    };
    auto attributeDescriptions = Vertex::getAttributeDescriptions();
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
            .setVertexInputInfo(bindingDescriptions, attributeDescriptions)
            .build(*device_);
}

void Renderer::initImGui() {
    std::array<vk::DescriptorPoolSize, 11> poolSizes = {
        vk::DescriptorPoolSize{             vk::DescriptorType::eSampler, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, 1000},
        vk::DescriptorPoolSize{        vk::DescriptorType::eSampledImage, 1000},
        vk::DescriptorPoolSize{        vk::DescriptorType::eStorageImage, 1000},
        vk::DescriptorPoolSize{  vk::DescriptorType::eUniformTexelBuffer, 1000},
        vk::DescriptorPoolSize{  vk::DescriptorType::eStorageTexelBuffer, 1000},
        vk::DescriptorPoolSize{       vk::DescriptorType::eUniformBuffer, 1000},
        vk::DescriptorPoolSize{       vk::DescriptorType::eStorageBuffer, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eUniformBufferDynamic, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eStorageBufferDynamic, 1000},
        vk::DescriptorPoolSize{     vk::DescriptorType::eInputAttachment, 1000}
    };

    // Create descriptor pool.
    vk::DescriptorPoolCreateInfo poolInfo{
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = 1000,
        .poolSizeCount = poolSizes.size(),
        .pPoolSizes = poolSizes.data()
    };

    imguiDescriptorPool_ = device_->getDevice().createDescriptorPool(poolInfo);


    // Setup Dear ImGui context.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // Setup platform/renderer backends.
    ImGui_ImplGlfw_InitForVulkan(window_.getWindow(), true);
    vk::Format colorFormat = viewport_.getSwapChainImageFormat();
    vk::Format depthFormat = viewport_.getDepthFormat();
    ImGui_ImplVulkan_InitInfo initInfo {
        .Instance = *instance_.getInstance(),
        .PhysicalDevice = *device_->getPhysicalDevice(),
        .Device = *device_->getDevice(),
        .QueueFamily = device_->getQueue().familyIndex,
        .Queue = *device_->getQueue().queue,
        .DescriptorPool = *imguiDescriptorPool_,
        .MinImageCount = 2,
        .ImageCount = 2,
        .UseDynamicRendering = true,
        .PipelineRenderingCreateInfo = vk::PipelineRenderingCreateInfo{
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &colorFormat,
            .depthAttachmentFormat = depthFormat,
        }
    };
    ImGui_ImplVulkan_Init(&initInfo);

    ImGui_ImplVulkan_CreateFontsTexture();
}

}
