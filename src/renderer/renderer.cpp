#include "renderer/renderer.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include "core/io/file.h"
#include "renderer/vma/buffer.h"
#include "renderer/vulkan_usage.h"
#include "pch.h"

namespace yuubi {

vk::VertexInputBindingDescription Vertex::getBindingDescription() {
    vk::VertexInputBindingDescription bindingDescription{
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = vk::VertexInputRate::eVertex};

    return bindingDescription;
}

std::array<vk::VertexInputAttributeDescription, 2>
Vertex::getAttributeDescriptions() {
    std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions;
    attributeDescriptions[0] = {.location = 0,
                                .binding = 0,
                                .format = vk::Format::eR32G32Sfloat,
                                .offset = offsetof(Vertex, pos)};

    attributeDescriptions[1] = {.location = 1,
                                .binding = 0,
                                .format = vk::Format::eR32G32B32Sfloat,
                                .offset = offsetof(Vertex, color)};

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

    createImmediateCommandBuffer();
    createVertexBuffer();
    createIndexBuffer();
    createGraphicsPipeline();
}

Renderer::~Renderer() { device_->getDevice().waitIdle(); }

void Renderer::draw() {
    viewport_.doFrame([this](const Frame& frame, const SwapchainImage& image) {
        vk::CommandBufferBeginInfo beginInfo{};
        frame.commandBuffer.begin(beginInfo);

        transitionImage(frame.commandBuffer, image.image,
                        vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);

        /*
        transitionImage(frame.commandBuffer,
                        viewport_.getDepthImage().getImage(),
                        vk::ImageLayout::eUndefined,
                        vk::ImageLayout::eDepthAttachmentOptimal);
        */

        vk::RenderingAttachmentInfo colorAttachmentInfo{
            .imageView = image.imageView,
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = {{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}}}};

        vk::RenderingAttachmentInfo depthAttachmentInfo{
            .imageView = viewport_.getDepthImageView(),
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
            // .pDepthAttachment = &depthAttachmentInfo,
        };

        frame.commandBuffer.beginRendering(renderInfo);
        {
            frame.commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                             graphicsPipeline_);

            frame.commandBuffer.bindVertexBuffers(
                0, {*vertexBuffer_.getBuffer()}, {0});
            frame.commandBuffer.bindIndexBuffer(indexBuffer_.getBuffer(), 0,
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
            auto view = glm::lookAt(glm::vec3(2.0f), glm::vec3(0.0f),
                                    glm::vec3(0.0f, 0.0f, 1.0f));

            auto projection = glm::perspective(
                glm::radians(45.0f),
                viewport_.getExtent().width /
                    static_cast<float>(viewport_.getExtent().height),
                0.1f, 10.0f);
            projection[1][1] *= -1.0f;

            auto mvp = projection * view;

            frame.commandBuffer.pushConstants<glm::mat4>(
                pipelineLayout_, vk::ShaderStageFlagBits::eVertex, 0,
                {mvp});

            vk::Viewport viewport{
                .x = 0.0f,
                .y = 0.0f,
                .width = static_cast<float>(viewport_.getExtent().width),
                .height = static_cast<float>(viewport_.getExtent().height),
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

        vk::Semaphore waitSemaphores[]{frame.imageAvailable};
        vk::PipelineStageFlags waitStages[]{
            vk::PipelineStageFlagBits::eColorAttachmentOutput};

        vk::Semaphore signalSemaphores[]{frame.renderFinished};

        vk::SubmitInfo submitInfo{.waitSemaphoreCount = 1,
                                  .pWaitSemaphores = waitSemaphores,
                                  .pWaitDstStageMask = waitStages,
                                  .commandBufferCount = 1,
                                  .pCommandBuffers = &*frame.commandBuffer,
                                  .signalSemaphoreCount = 1,
                                  .pSignalSemaphores = signalSemaphores};

        device_->getQueue().queue.submit({submitInfo}, frame.inFlight);
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
    auto vertShaderCode = yuubi::readFile("shaders/shader.vert.spv");
    auto fragShaderCode = yuubi::readFile("shaders/shader.frag.spv");

    vk::raii::ShaderModule vertShaderModule(
        device_->getDevice(),
        {.codeSize = vertShaderCode.size(),
         .pCode = reinterpret_cast<const uint32_t*>(vertShaderCode.data())});
    vk::raii::ShaderModule fragShaderModule(
        device_->getDevice(),
        {.codeSize = fragShaderCode.size(),
         .pCode = reinterpret_cast<const uint32_t*>(fragShaderCode.data())});

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
        .stage = vk::ShaderStageFlagBits::eVertex,
        .module = vertShaderModule,
        .pName = "main"};

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
        .stage = vk::ShaderStageFlagBits::eFragment,
        .module = fragShaderModule,
        .pName = "main"};

    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                        fragShaderStageInfo};

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDescription,
        .vertexAttributeDescriptionCount =
            static_cast<uint32_t>(attributeDescriptions.size()),
        .pVertexAttributeDescriptions = attributeDescriptions.data()};

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
        .topology = vk::PrimitiveTopology::eTriangleList,
        .primitiveRestartEnable = vk::False};

    std::vector<vk::DynamicState> dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
    };

    vk::PipelineDynamicStateCreateInfo dynamicState{
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()};

    vk::PipelineViewportStateCreateInfo viewportState{
        .viewportCount = 1,
        .scissorCount = 1,
    };

    vk::PipelineRasterizationStateCreateInfo rasterizer{
        .depthClampEnable = vk::False,
        .rasterizerDiscardEnable = vk::False,
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eBack,
        .frontFace = vk::FrontFace::eCounterClockwise,
        .depthBiasEnable = vk::False,
        .lineWidth = 1.0f,
    };

    vk::PipelineMultisampleStateCreateInfo multisampling{
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
        .sampleShadingEnable = vk::False,
        .minSampleShading = 1.0f,
    };

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable = vk::False,
        .colorWriteMask =
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
    };

    vk::PipelineColorBlendStateCreateInfo colorBlending{
        .logicOpEnable = vk::False,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment};

    vk::PushConstantRange pushConstantRange{
        .stageFlags = vk::ShaderStageFlagBits::eVertex,
        .offset = 0,
        .size = sizeof(glm::mat4),
    };

    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo{
        .setLayoutCount = 0,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange};

    pipelineLayout_ = vk::raii::PipelineLayout(device_->getDevice(),
                                               pipelineLayoutCreateInfo);

    vk::PipelineDepthStencilStateCreateInfo depthStencil{
        .depthTestEnable = vk::True,
        .depthWriteEnable = vk::True,
        .depthCompareOp = vk::CompareOp::eLess,
        .depthBoundsTestEnable = vk::False,
        .stencilTestEnable = vk::False,
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,
    };

    vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo{
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &viewport_.getSwapChainImageFormat(),
        // .depthAttachmentFormat = viewport_.getDepthFormat(),
    };
    vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo{
        .pNext = &pipelineRenderingCreateInfo,
        .stageCount = 2,
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        // .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = pipelineLayout_,
        .subpass = 0};

    graphicsPipeline_ = vk::raii::Pipeline(device_->getDevice(), nullptr,
                                           graphicsPipelineCreateInfo);
}

void Renderer::createVertexBuffer() {
    vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    // Create staging buffer.
    vk::BufferCreateInfo stagingBufferCreateInfo{
        .size = bufferSize,
        .usage = vk::BufferUsageFlagBits::eTransferSrc,
    };

    vma::AllocationCreateInfo stagingBufferAllocCreateInfo{
        .flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite |
                 vma::AllocationCreateFlagBits::eMapped,
        .usage = vma::MemoryUsage::eAuto,
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

    vma::AllocationCreateInfo vertexBufferAllocCreateInfo{
        .usage = vma::MemoryUsage::eAuto};

    vertexBuffer_ = device_->createBuffer(vertexBufferCreateInfo,
                                          vertexBufferAllocCreateInfo);

    submitImmediateCommands([this, &stagingBuffer, bufferSize](
                                const vk::raii::CommandBuffer& commandBuffer) {
        vk::BufferCopy copyRegion{.size = bufferSize};
        commandBuffer.copyBuffer(stagingBuffer.getBuffer(),
                                 vertexBuffer_.getBuffer(), {copyRegion});
    });
}

void Renderer::createIndexBuffer() {
    vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    // Create staging buffer.
    vk::BufferCreateInfo stagingBufferCreateInfo{
        .size = bufferSize,
        .usage = vk::BufferUsageFlagBits::eTransferSrc,
    };

    vma::AllocationCreateInfo stagingBufferAllocCreateInfo{
        .flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite |
                 vma::AllocationCreateFlagBits::eMapped,
        .usage = vma::MemoryUsage::eAuto,
    };

    Buffer stagingBuffer = device_->createBuffer(stagingBufferCreateInfo,
                                                 stagingBufferAllocCreateInfo);

    // Copy vertex data onto mapped memory in staging buffer.
    std::memcpy(stagingBuffer.getMappedMemory(), indices.data(),
                static_cast<size_t>(bufferSize));

    // Create vertex buffer.
    vk::BufferCreateInfo indexBufferCreateInfo{
        .size = bufferSize,
        .usage = vk::BufferUsageFlagBits::eIndexBuffer |
                 vk::BufferUsageFlagBits::eTransferDst};

    vma::AllocationCreateInfo indexBufferAllocCreateInfo{
        .usage = vma::MemoryUsage::eAuto};

    indexBuffer_ = device_->createBuffer(indexBufferCreateInfo,
                                         indexBufferAllocCreateInfo);

    submitImmediateCommands([this, &stagingBuffer, bufferSize](
                                const vk::raii::CommandBuffer& commandBuffer) {
        vk::BufferCopy copyRegion{.size = bufferSize};
        commandBuffer.copyBuffer(stagingBuffer.getBuffer(),
                                 indexBuffer_.getBuffer(), {copyRegion});
    });
}

void Renderer::createImmediateCommandBuffer() {
    immediateCommandPool_ = vk::raii::CommandPool{
        device_->getDevice(),
        {.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
         .queueFamilyIndex = device_->getQueue().familyIndex}};

    // TODO: wtf?
    vk::CommandBufferAllocateInfo allocInfo{
        .commandPool = immediateCommandPool_,
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
        .commandBuffer = immediateCommandBuffer_,
    };
    device_->getQueue().queue.submit2(
        {vk::SubmitInfo2{
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &commandBufferSubmitInfo,
        }},
        immediateCommandFence_);

    device_->getDevice().waitForFences({immediateCommandFence_}, vk::True,
                                       std::numeric_limits<uint64_t>::max());
}
}
