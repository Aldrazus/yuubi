#include "renderer/renderer.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include "core/io/file.h"
#include "renderer/camera.h"
#include "renderer/vma/buffer.h"
#include "renderer/vulkan_usage.h"
#include "renderer/pipeline_builder.h"
#include "renderer/descriptor_layout_builder.h"
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
    descriptorAllocator_ = DescriptorAllocator(device_);

    createImmediateCommandBuffer();
    createVertexBuffer();
    createIndexBuffer();
    createDescriptor();
    createGraphicsPipeline();
    initImGui();
}

Renderer::~Renderer() {
    device_->getDevice().waitIdle();
    ImGui_ImplVulkan_Shutdown();
}

void Renderer::draw(const Camera& camera) {
    viewport_.doFrame([this, &camera](const Frame& frame,
                                      const SwapchainImage& image) {
        vk::CommandBufferBeginInfo beginInfo{};
        frame.commandBuffer.begin(beginInfo);

        transitionImage(frame.commandBuffer, image.image,
                        vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);

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

            frame.commandBuffer.drawIndexed(
                static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
        }
        frame.commandBuffer.endRendering();

        transitionImage(frame.commandBuffer, image.image,
                        vk::ImageLayout::eGeneral,
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

void Renderer::transitionImage(const vk::raii::CommandBuffer& commandBuffer,
                               const vk::Image& image,
                               const vk::ImageLayout& currentLayout,
                               const vk::ImageLayout& newLayout) {
    vk::ImageMemoryBarrier2 imageBarrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .srcAccessMask = vk::AccessFlagBits2::eMemoryWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .dstAccessMask = vk::AccessFlagBits2::eMemoryWrite |
                         vk::AccessFlagBits2::eMemoryRead,
        .oldLayout = currentLayout,
        .newLayout = newLayout,
        .image = image,
        .subresourceRange{
            .aspectMask = newLayout == vk::ImageLayout::eDepthAttachmentOptimal
                              ? vk::ImageAspectFlagBits::eDepth
                              : vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = vk::RemainingMipLevels,
            .baseArrayLayer = 0,
            .layerCount = vk::RemainingArrayLayers},
    };

    vk::DependencyInfo dependencyInfo{.imageMemoryBarrierCount = 1,
                                      .pImageMemoryBarriers = &imageBarrier};

    commandBuffer.pipelineBarrier2(dependencyInfo);
}

void Renderer::createGraphicsPipeline() {
    auto vertShader = loadShader("shaders/shader.vert.spv", *device_);
    auto fragShader = loadShader("shaders/shader.frag.spv", *device_);

    std::vector<vk::PushConstantRange> pushConstantRanges = {vk::PushConstantRange{
        .stageFlags = vk::ShaderStageFlagBits::eVertex,
        .offset = 0,
        .size = sizeof(PushConstants),
    }};

    std::vector<vk::DescriptorSetLayout> setLayouts = {*descriptorSetLayout_};
    
    pipelineLayout_ = createPipelineLayout(*device_, setLayouts, pushConstantRanges);
    PipelineBuilder builder(pipelineLayout_);
    std::array<vk::VertexInputBindingDescription, 1> bindingDescriptions{
        Vertex::getBindingDescription()
    };
    auto attributeDescriptions = Vertex::getAttributeDescriptions();
    graphicsPipeline_ = builder
        .setShaders(vertShader, fragShader)
        .setInputTopology(vk::PrimitiveTopology::eTriangleList)
        .setPolygonMode(vk::PolygonMode::eFill)
        .setCullMode(vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise)
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

    submitImmediateCommands([this, &stagingBuffer, bufferSize](
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

    submitImmediateCommands([this, &stagingBuffer, bufferSize](
                                const vk::raii::CommandBuffer& commandBuffer) {
        vk::BufferCopy copyRegion{.size = bufferSize};
        commandBuffer.copyBuffer(*stagingBuffer.getBuffer(),
                                 *indexBuffer_.getBuffer(), {copyRegion});
    });
}

void Renderer::createDescriptor() {
    DescriptorLayoutBuilder layoutBuilder(device_);

    vk::DescriptorBindingFlags bindingFlags = vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind;

    descriptorSetLayout_ = layoutBuilder
        .addBinding(vk::DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = 1024,
            .stageFlags = vk::ShaderStageFlagBits::eFragment,
        })
        .build(vk::DescriptorSetLayoutBindingFlagsCreateInfo{
            .bindingCount = 1,
            .pBindingFlags = &bindingFlags
        }, vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool);

    descriptorAllocator_.allocate(descriptorSetLayout_);
}

void Renderer::createImmediateCommandBuffer() {
    immediateCommandPool_ = vk::raii::CommandPool{
        device_->getDevice(),
        {.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
         .queueFamilyIndex = device_->getQueue().familyIndex}};

    // TODO: wtf?
    vk::CommandBufferAllocateInfo allocInfo{
        .commandPool = *immediateCommandPool_,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1};
    immediateCommandBuffer_ =
        std::move(device_->getDevice().allocateCommandBuffers(allocInfo)[0]);

    immediateCommandFence_ = vk::raii::Fence{
        device_->getDevice(),
        vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled}};
}

void Renderer::submitImmediateCommands(
    std::function<void(const vk::raii::CommandBuffer& commandBuffer)>&&
        function) {
    device_->getDevice().resetFences(*immediateCommandFence_);
    immediateCommandBuffer_.reset();

    immediateCommandBuffer_.begin(vk::CommandBufferBeginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    function(immediateCommandBuffer_);

    immediateCommandBuffer_.end();

    vk::CommandBufferSubmitInfo commandBufferSubmitInfo{
        .commandBuffer = *immediateCommandBuffer_,
    };
    device_->getQueue().queue.submit2(
        {vk::SubmitInfo2{
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &commandBufferSubmitInfo,
        }},
        *immediateCommandFence_);

    device_->getDevice().waitForFences({*immediateCommandFence_}, vk::True,
                                       std::numeric_limits<uint64_t>::max());
}

void Renderer::initImGui() {
    IMGUI_CHECKVERSION();
    std::array<vk::DescriptorPoolSize, 11> poolSizes{
        vk::DescriptorPoolSize{.type = vk::DescriptorType::eSampler,
                               .descriptorCount = 1000},
        vk::DescriptorPoolSize{
            .type = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = 1000},
        vk::DescriptorPoolSize{.type = vk::DescriptorType::eSampledImage,
                               .descriptorCount = 1000},
        vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageImage,
                               .descriptorCount = 1000},
        vk::DescriptorPoolSize{.type = vk::DescriptorType::eUniformTexelBuffer,
                               .descriptorCount = 1000},
        vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageTexelBuffer,
                               .descriptorCount = 1000},
        vk::DescriptorPoolSize{.type = vk::DescriptorType::eUniformBuffer,
                               .descriptorCount = 1000},
        vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageBuffer,
                               .descriptorCount = 1000},
        vk::DescriptorPoolSize{
            .type = vk::DescriptorType::eUniformBufferDynamic,
            .descriptorCount = 1000},
        vk::DescriptorPoolSize{
            .type = vk::DescriptorType::eStorageBufferDynamic,
            .descriptorCount = 1000},
        vk::DescriptorPoolSize{.type = vk::DescriptorType::eInputAttachment,
                               .descriptorCount = 1000},
    };

    imguiPool_ = vk::raii::DescriptorPool(
        device_->getDevice(),
        vk::DescriptorPoolCreateInfo{
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = 1000,
            .poolSizeCount = poolSizes.size(),
            .pPoolSizes = poolSizes.data(),
        });

    ImGui::CreateContext();

    ImGui_ImplGlfw_InitForVulkan(window_.getWindow(), true);

    ImGui_ImplVulkan_InitInfo initInfo{
        .Instance = *instance_.getInstance(),
        .PhysicalDevice = *device_->getPhysicalDevice(),
        .Device = *device_->getDevice(),
        .QueueFamily = device_->getQueue().familyIndex,
        .Queue = *device_->getQueue().queue,
        .DescriptorPool = *imguiPool_,
        .MinImageCount = 3,
        .ImageCount = 3,
        .PipelineCache = nullptr,
        .UseDynamicRendering = true,
        .PipelineRenderingCreateInfo = vk::PipelineRenderingCreateInfo{
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &viewport_.getSwapChainImageFormat()}};

    ImGui_ImplVulkan_Init(&initInfo);

    ImGui_ImplVulkan_CreateFontsTexture();
}
}
