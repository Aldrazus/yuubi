#include "renderer/vulkan/util.h"

namespace yuubi {

    // PERF: Image transition barriers should be handled via rendergraph
    void transitionImage(
        const vk::raii::CommandBuffer& commandBuffer, const vk::Image& image, const vk::ImageLayout& currentLayout,
        const vk::ImageLayout& newLayout
    ) {
        vk::ImageMemoryBarrier2 imageBarrier{
            .oldLayout = currentLayout,
            .newLayout = newLayout,
            .image = image,
            .subresourceRange{
                              .aspectMask = newLayout == vk::ImageLayout::eDepthAttachmentOptimal ? vk::ImageAspectFlagBits::eDepth
                                                                                    : vk::ImageAspectFlagBits::eColor,
                              .baseMipLevel = 0,
                              .levelCount = vk::RemainingMipLevels,
                              .baseArrayLayer = 0,
                              .layerCount = vk::RemainingArrayLayers
            },
        };

        if (currentLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
            imageBarrier.setSrcAccessMask(vk::AccessFlagBits2::eNone);
            imageBarrier.setDstAccessMask(vk::AccessFlagBits2::eTransferWrite);

            imageBarrier.setSrcStageMask(vk::PipelineStageFlagBits2::eTopOfPipe);
            imageBarrier.setDstStageMask(vk::PipelineStageFlagBits2::eTransfer);
        } else if (currentLayout == vk::ImageLayout::eTransferDstOptimal &&
                   newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
            imageBarrier.setSrcAccessMask(vk::AccessFlagBits2::eTransferWrite);
            imageBarrier.setDstAccessMask(vk::AccessFlagBits2::eShaderRead);

            imageBarrier.setSrcStageMask(vk::PipelineStageFlagBits2::eTransfer);
            imageBarrier.setDstStageMask(vk::PipelineStageFlagBits2::eFragmentShader);
        } else if (currentLayout == vk::ImageLayout::eUndefined &&
                   newLayout == vk::ImageLayout::eDepthAttachmentOptimal) {
            imageBarrier.setSrcAccessMask(vk::AccessFlagBits2::eNone);
            imageBarrier.setDstAccessMask(
                vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite
            );

            imageBarrier.setSrcStageMask(vk::PipelineStageFlagBits2::eTopOfPipe);
            imageBarrier.setDstStageMask(vk::PipelineStageFlagBits2::eEarlyFragmentTests);
        } else if (currentLayout == vk::ImageLayout::eColorAttachmentOptimal &&
                   newLayout == vk::ImageLayout::ePresentSrcKHR) {
            imageBarrier.setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite);
            // imageBarrier.setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite);

            imageBarrier.setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput);
            imageBarrier.setDstStageMask(vk::PipelineStageFlagBits2::eBottomOfPipe);
        } else if (currentLayout == vk::ImageLayout::eUndefined &&
                   newLayout == vk::ImageLayout::eColorAttachmentOptimal) {
            // imageBarrier.setSrcAccessMask(vk::AccessFlagBits2::eNone);
            imageBarrier.setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite);

            imageBarrier.setSrcStageMask(vk::PipelineStageFlagBits2::eTopOfPipe);
            imageBarrier.setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput);
        } else if (currentLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::ePresentSrcKHR) {
            // TODO: temporary
            imageBarrier.setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite);
            // imageBarrier.setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite);

            imageBarrier.setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput);
            imageBarrier.setDstStageMask(vk::PipelineStageFlagBits2::eBottomOfPipe);
        } else {
            throw std::invalid_argument("Unsupported layout transition!");
        }

        const vk::DependencyInfo dependencyInfo{.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &imageBarrier};

        commandBuffer.pipelineBarrier2(dependencyInfo);
    }

}
