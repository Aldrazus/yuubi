#include "renderer/renderer.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_raii.hpp>
#include "core/io/image.h"

#include "core/io/file.h"
#include "renderer/camera.h"
#include "renderer/vma/buffer.h"
#include "renderer/vulkan_usage.h"
#include "renderer/pipeline_builder.h"
#include "renderer/descriptor_layout_builder.h"
#include "renderer/bindless_set_manager.h"
#include "renderer/vulkan/util.h"
#include "pch.h"

namespace yuubi {

const vk::VertexInputBindingDescription Vertex::getBindingDescription() {
    vk::VertexInputBindingDescription bindingDescription{
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = vk::VertexInputRate::eVertex};

    return bindingDescription;
}
const std::array<vk::VertexInputAttributeDescription, 4>
Vertex::getAttributeDescriptions() {
    std::array<vk::VertexInputAttributeDescription, 4> attributeDescriptions;
    attributeDescriptions[0] = {.location = 0,
                                .binding = 0,
                                .format = vk::Format::eR32G32Sfloat,
                                .offset = offsetof(Vertex, position)};

    attributeDescriptions[1] = {.location = 1,
                                .binding = 0,
                                .format = vk::Format::eR32G32B32Sfloat,
                                .offset = offsetof(Vertex, normal)};

    attributeDescriptions[2] = {.location = 2,
                                .binding = 0,
                                .format = vk::Format::eR32G32B32Sfloat,
                                .offset = offsetof(Vertex, color)};
    attributeDescriptions[3] = {.location = 3,
                                .binding = 0,
                                .format = vk::Format::eR32G32Sfloat,
                                .offset = offsetof(Vertex, uv)};

    return attributeDescriptions;
}

Renderer::Renderer(const Window& window) : window_(window) {
    instance_ = Instance{context_};

    VkSurfaceKHR tmp;
    if (glfwCreateWindowSurface(
            static_cast<VkInstance>(*instance_.getInstance()),
            window_.getWindow(), nullptr, &tmp) != VK_SUCCESS) {
        UB_ERROR("Unable to create window surface!");
    }

    surface_ = std::make_shared<vk::raii::SurfaceKHR>(instance_, tmp);
    device_ = std::make_shared<Device>(instance_, *surface_);
    viewport_ = Viewport{surface_, device_};
    bindlessSetManager_ = BindlessSetManager(device_);

    createVertexBuffer();
    createIndexBuffer();
    texture_ = Texture{*device_, "textures/texture.jpg"};
    bindlessSetManager_.addImage(texture_);
    createGraphicsPipeline();
}

Renderer::~Renderer() {
    device_->getDevice().waitIdle();
}

void Renderer::draw(const Camera& camera) {
    viewport_.doFrame([this, &camera](const Frame& frame,
                                      const SwapchainImage& image) {
        vk::CommandBufferBeginInfo beginInfo{};
        frame.commandBuffer.begin(beginInfo);

        // Transition swapchain image layout to COLOR_ATTACHMENT_OPTIMAL before rendering
        transitionImage(frame.commandBuffer, image.image,
                        vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);

        // Transition depth buffer layout to DEPTH_ATTACHMENT_OPTIMAL before rendering
        transitionImage(frame.commandBuffer,
                        *viewport_.getDepthImage().getImage(),
                        vk::ImageLayout::eUndefined,
                        vk::ImageLayout::eDepthAttachmentOptimal);

        vk::RenderingAttachmentInfo colorAttachmentInfo{
            .imageView = *image.imageView,
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = {{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}}}};

        vk::RenderingAttachmentInfo depthAttachmentInfo{
            .imageView = *viewport_.getDepthImageView(),
            .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = {.depthStencil = {1.0f, 0}}};

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
            frame.commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                             *graphicsPipeline_);

            frame.commandBuffer.bindVertexBuffers(
                0, {*vertexBuffer_.getBuffer()}, {0});

            frame.commandBuffer.bindIndexBuffer(*indexBuffer_.getBuffer(), 0,
                                                vk::IndexType::eUint16);

            static auto startTime = std::chrono::high_resolution_clock::now();
            auto currentTime = std::chrono::high_resolution_clock::now();

            float time =
                std::chrono::duration<float, std::chrono::seconds::period>(
                    currentTime - startTime)
                    .count();
            auto model =
                glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f),
                            glm::vec3(0.0f, 0.0f, 1.0f));

            frame.commandBuffer.pushConstants<PushConstants>(
                *pipelineLayout_, vk::ShaderStageFlagBits::eVertex, 0,
                {PushConstants{camera.getViewProjectionMatrix() * model}});

            // NOTE: Viewport is flipped vertically to match OpenGL/GLM's clip
            // coordinate system where the origin is at the bottom left and the
            // y-axis points upwards.
            vk::Viewport viewport{
                .x = 0.0f,
                .y = static_cast<float>(viewport_.getExtent().height),
                .width = static_cast<float>(viewport_.getExtent().width),
                .height = -static_cast<float>(viewport_.getExtent().height),
                .minDepth = 0.0f,
                .maxDepth = 1.0f};

            frame.commandBuffer.setViewport(0, {viewport});

            vk::Rect2D scissor{.offset = {0, 0},
                               .extent = viewport_.getExtent()};

            frame.commandBuffer.setScissor(0, {scissor});

            frame.commandBuffer.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics, *pipelineLayout_, 0, {*bindlessSetManager_.getDescriptorSet()}, {});

            frame.commandBuffer.drawIndexed(
                static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
        }
        frame.commandBuffer.endRendering();

        // Transition swapchain image layout to PRESENT_SRC before presenting
        transitionImage(frame.commandBuffer, image.image,
                        vk::ImageLayout::eColorAttachmentOptimal,
                        vk::ImageLayout::ePresentSrcKHR);

        frame.commandBuffer.end();

        vk::Semaphore waitSemaphores[]{*frame.imageAvailable};
        vk::PipelineStageFlags waitStages[]{
            vk::PipelineStageFlagBits::eColorAttachmentOutput};

        vk::Semaphore signalSemaphores[]{*frame.renderFinished};

        vk::SubmitInfo submitInfo{.waitSemaphoreCount = 1,
                                  .pWaitSemaphores = waitSemaphores,
                                  .pWaitDstStageMask = waitStages,
                                  .commandBufferCount = 1,
                                  .pCommandBuffers = &*frame.commandBuffer,
                                  .signalSemaphoreCount = 1,
                                  .pSignalSemaphores = signalSemaphores};

        device_->getQueue().queue.submit({submitInfo}, *frame.inFlight);
    });
}

void Renderer::createGraphicsPipeline() {
    auto vertShader = loadShader("shaders/shader.vert.spv", *device_);
    auto fragShader = loadShader("shaders/shader.frag.spv", *device_);

    std::vector<vk::PushConstantRange> pushConstantRanges = {
        vk::PushConstantRange{
            .stageFlags = vk::ShaderStageFlagBits::eVertex,
            .offset = 0,
            .size = sizeof(PushConstants),
        }};

    std::vector<vk::DescriptorSetLayout> setLayouts = {*bindlessSetManager_.getDescriptorSetLayout()};

    pipelineLayout_ =
        createPipelineLayout(*device_, setLayouts, pushConstantRanges);
    PipelineBuilder builder(pipelineLayout_);
    std::array<vk::VertexInputBindingDescription, 1> bindingDescriptions{
        Vertex::getBindingDescription()};
    auto attributeDescriptions = Vertex::getAttributeDescriptions();
    graphicsPipeline_ =
        builder.setShaders(vertShader, fragShader)
            .setInputTopology(vk::PrimitiveTopology::eTriangleList)
            .setPolygonMode(vk::PolygonMode::eFill)
            .setCullMode(vk::CullModeFlagBits::eBack,
                         vk::FrontFace::eCounterClockwise)
            .setMultisamplingNone()
            .disableBlending()
            .disableDepthTest()
            .setColorAttachmentFormat(viewport_.getSwapChainImageFormat())
            .setDepthFormat(viewport_.getDepthFormat())
            .setVertexInputInfo(bindingDescriptions, attributeDescriptions)
            .build(*device_);
}

void Renderer::createVertexBuffer() {
    vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    // Create staging buffer.
    vk::BufferCreateInfo stagingBufferCreateInfo{
        .size = bufferSize,
        .usage = vk::BufferUsageFlagBits::eTransferSrc,
    };

    VmaAllocationCreateInfo stagingBufferAllocCreateInfo{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_CPU_ONLY,
    };

    Buffer stagingBuffer = device_->createBuffer(stagingBufferCreateInfo,
                                                 stagingBufferAllocCreateInfo);

    // Copy vertex data onto mapped memory in staging buffer.
    std::memcpy(stagingBuffer.getMappedMemory(), vertices.data(),
                static_cast<size_t>(bufferSize));

    // Create vertex buffer.
    vk::BufferCreateInfo vertexBufferCreateInfo{
        .size = bufferSize,
        .usage = vk::BufferUsageFlagBits::eVertexBuffer |
                 vk::BufferUsageFlagBits::eTransferDst};

    VmaAllocationCreateInfo vertexBufferAllocCreateInfo{
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
    };

    vertexBuffer_ = device_->createBuffer(vertexBufferCreateInfo,
                                          vertexBufferAllocCreateInfo);

    device_->submitImmediateCommands([this, &stagingBuffer, bufferSize](
                                const vk::raii::CommandBuffer& commandBuffer) {
        vk::BufferCopy copyRegion{.size = bufferSize};
        commandBuffer.copyBuffer(*stagingBuffer.getBuffer(),
                                 *vertexBuffer_.getBuffer(), {copyRegion});
    });
}

void Renderer::createIndexBuffer() {
    vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    // Create staging buffer.
    vk::BufferCreateInfo stagingBufferCreateInfo{
        .size = bufferSize,
        .usage = vk::BufferUsageFlagBits::eTransferSrc,
    };

    VmaAllocationCreateInfo stagingBufferAllocCreateInfo{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    Buffer stagingBuffer = device_->createBuffer(stagingBufferCreateInfo,
                                                 stagingBufferAllocCreateInfo);

    // Copy vertex data onto mapped memory in staging buffer.
    std::memcpy(stagingBuffer.getMappedMemory(), indices.data(),
                static_cast<size_t>(bufferSize));

    // Create index buffer.
    vk::BufferCreateInfo indexBufferCreateInfo{
        .size = bufferSize,
        .usage = vk::BufferUsageFlagBits::eIndexBuffer |
                 vk::BufferUsageFlagBits::eTransferDst};

    VmaAllocationCreateInfo indexBufferAllocCreateInfo{
        .usage = VMA_MEMORY_USAGE_GPU_ONLY};

    indexBuffer_ = device_->createBuffer(indexBufferCreateInfo,
                                         indexBufferAllocCreateInfo);

    device_->submitImmediateCommands([this, &stagingBuffer, bufferSize](
                                const vk::raii::CommandBuffer& commandBuffer) {
        vk::BufferCopy copyRegion{.size = bufferSize};
        commandBuffer.copyBuffer(*stagingBuffer.getBuffer(),
                                 *indexBuffer_.getBuffer(), {copyRegion});
    });
}

}
