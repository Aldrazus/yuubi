#include "renderer/renderer.h"

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

namespace yuubi {

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
}

Renderer::~Renderer() {
    device_->getDevice().waitIdle();
}

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
            .pDepthAttachment = &depthAttachmentInfo,
        };

        frame.commandBuffer.beginRendering(renderInfo);
        {
            // TODO 
        }
        frame.commandBuffer.endRendering();

        transitionImage(frame.commandBuffer, image.image,
                        vk::ImageLayout::eGeneral,
                        vk::ImageLayout::ePresentSrcKHR);

        frame.commandBuffer.end();

        vk::Semaphore waitSemaphores[]{frame.imageAvailable};
        vk::PipelineStageFlags waitStages[]{
            vk::PipelineStageFlagBits::eColorAttachmentOutput};

        vk::Semaphore signalSemaphores[] { frame.renderFinished };

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

}
