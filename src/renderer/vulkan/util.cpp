#include "renderer/vulkan/util.h"

namespace yuubi {

    void transitionImage(
        const vk::raii::CommandBuffer& commandBuffer, const vk::Image& image, const vk::ImageLayout& currentLayout,
        const vk::ImageLayout& newLayout
    ) {
        vk::ImageMemoryBarrier2 imageBarrier{
            .oldLayout = currentLayout,
            .newLayout = newLayout,
            .image = image,
            .subresourceRange{
                              .aspectMask = vk::ImageAspectFlagBits::eColor,
                              .baseMipLevel = 0,
                              .levelCount = vk::RemainingMipLevels,
                              .baseArrayLayer = 0,
                              .layerCount = vk::RemainingArrayLayers
            },
        };

        if (currentLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eGeneral) {
            imageBarrier.setSrcStageMask(vk::PipelineStageFlagBits2::eTopOfPipe);
            imageBarrier.setDstStageMask(vk::PipelineStageFlagBits2::eTransfer);
            imageBarrier.setDstAccessMask(vk::AccessFlagBits2::eTransferWrite);
        } else if (currentLayout == vk::ImageLayout::eGeneral && newLayout == vk::ImageLayout::ePresentSrcKHR) {
            imageBarrier.setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput);
            imageBarrier.setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite);
            imageBarrier.setDstStageMask(vk::PipelineStageFlagBits2::eBottomOfPipe);
        } else {
            throw std::invalid_argument("Unsupported layout transition!");
        }

        const vk::DependencyInfo dependencyInfo{.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &imageBarrier};

        commandBuffer.pipelineBarrier2(dependencyInfo);
    }

}
