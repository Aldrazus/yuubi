#include "renderer/device.h"
#include "renderer/vma/image.h"
#include "renderer/vma/allocator.h"
#include "renderer/vulkan/util.h"
#include <glm/glm.hpp>

namespace yuubi {

    Image::Image(Allocator* allocator, const ImageCreateInfo& createInfo) :
        format_(createInfo.format), mipLevels_(createInfo.mipLevels), allocator_(allocator) {
        vk::ImageCreateInfo imageInfo{
                .imageType = vk::ImageType::e2D,
                .format = createInfo.format,
                .extent = {.width = createInfo.width, .height = createInfo.height, .depth = 1},
                .mipLevels = createInfo.mipLevels,
                .arrayLayers = createInfo.arrayLayers,
                .samples = vk::SampleCountFlagBits::e1,
                .tiling = createInfo.tiling,
                .usage = createInfo.usage,
                .sharingMode = vk::SharingMode::eExclusive,
                .initialLayout = vk::ImageLayout::eUndefined,
        };

        if (createInfo.arrayLayers == 6) {
            imageInfo.setFlags(vk::ImageCreateFlagBits::eCubeCompatible);
        }

        vma::AllocationCreateInfo allocInfo{};

        auto [image, allocation] = allocator_->getAllocator().createImage(imageInfo, allocInfo);
        image_ = vk::raii::Image{allocator_->getDevice(), image};
        allocation_ = allocation;
    }

    Image::Image(Image&& rhs) noexcept :
        image_(std::exchange(rhs.image_, nullptr)), format_(std::exchange(rhs.format_, vk::Format::eUndefined)),
        mipLevels_(std::exchange(rhs.mipLevels_, 1)), allocator_(std::exchange(rhs.allocator_, nullptr)),
        allocation_(std::exchange(rhs.allocation_, nullptr)) {}

    Image& Image::operator=(Image&& rhs) noexcept {
        if (this != &rhs) {
            std::swap(allocator_, rhs.allocator_);
            std::swap(image_, rhs.image_);
            std::swap(format_, rhs.format_);
            std::swap(mipLevels_, rhs.mipLevels_);
            std::swap(allocation_, rhs.allocation_);
        }
        return *this;
    }

    Image::~Image() { destroy(); }

    void Image::destroy() {
        if (allocator_ != nullptr) {
            allocator_->getAllocator().destroyImage(image_.release(), allocation_);
        }
    }

    // PERF: Use staging buffer pool. Use dedicated transfer queue.
    Image createImageFromData(const Device& device, const ImageData& data) {
        // Create staging buffer.
        vk::DeviceSize imageSize = data.width * data.height * (data.numChannels == 3 ? 4 : data.numChannels);

        vk::BufferCreateInfo stagingBufferCreateInfo{
                .size = imageSize,
                .usage = vk::BufferUsageFlagBits::eTransferSrc,
        };

        vma::AllocationCreateInfo stagingBufferAllocCreateInfo{
                .flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite |
                         vma::AllocationCreateFlagBits::eMapped,
                .usage = vma::MemoryUsage::eAuto,
        };

        Buffer stagingBuffer = device.createBuffer(stagingBufferCreateInfo, stagingBufferAllocCreateInfo);

        if (data.numChannels == 3) {
            auto pixels =
                    std::span{
                            reinterpret_cast<const glm::u8vec3*>(data.pixels),
                            static_cast<size_t>(data.width * data.height)
                    } |
                    std::views::transform([](const auto& pixel) { return glm::u8vec4{pixel, 255}; });
            std::ranges::copy(pixels, static_cast<glm::u8vec4*>(stagingBuffer.getMappedMemory()));
        } else {
            auto pixels = std::span{data.pixels, static_cast<size_t>(data.width * data.height * data.numChannels)};
            std::ranges::copy(pixels, static_cast<unsigned char*>(stagingBuffer.getMappedMemory()));
        }

        // Create image.
        uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(data.width, data.height)))) + 1;

        Image image(
                &device.allocator(),
                ImageCreateInfo{
                        .width = data.width,
                        .height = data.height,
                        .format = data.format,
                        .tiling = vk::ImageTiling::eOptimal,
                        .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc |
                                 vk::ImageUsageFlagBits::eTransferDst,
                        .properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
                        .mipLevels = mipLevels
                }
        );

        // Upload data to image.
        device.submitImmediateCommands([&stagingBuffer, &image, &data,
                                        mipLevels](const vk::raii::CommandBuffer& commandBuffer) {
            transitionImage(
                    commandBuffer, *image.getImage(), vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal
            );

            vk::BufferImageCopy copyRegion{
                    .bufferOffset = 0,
                    .bufferRowLength = 0,
                    .bufferImageHeight = 0,
                    .imageSubresource =
                            vk::ImageSubresourceLayers{
                                                       .aspectMask = vk::ImageAspectFlagBits::eColor,
                                                       .mipLevel = 0,
                                                       .baseArrayLayer = 0,
                                                       .layerCount = 1
                            },
                    .imageOffset = {0, 0, 0},
                    .imageExtent = vk::Extent3D{.width = data.width, .height = data.height, .depth = 1}
            };
            commandBuffer.copyBufferToImage(
                    *stagingBuffer.getBuffer(), *image.getImage(), vk::ImageLayout::eTransferDstOptimal, {copyRegion}
            );

            // Generate mipmaps.
            vk::ImageMemoryBarrier2 barrier{
                    .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
                    .dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
                    .image = *image.getImage(),
                    .subresourceRange =
                            vk::ImageSubresourceRange{
                                                      .aspectMask = vk::ImageAspectFlagBits::eColor,
                                                      .levelCount = 1,
                                                      .baseArrayLayer = 0,
                                                      .layerCount = 1,
                                                      },
            };

            int32_t mipWidth = data.width;
            int32_t mipHeight = data.height;

            for (uint32_t i = 1; i < mipLevels; i++) {
                barrier.subresourceRange.baseMipLevel = i - 1;
                barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
                barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
                barrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
                barrier.dstAccessMask = vk::AccessFlagBits2::eTransferRead;
                barrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
                barrier.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;

                vk::DependencyInfo dependencyInfo{.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier};

                commandBuffer.pipelineBarrier2(dependencyInfo);

                std::array<vk::Offset3D, 2> srcOffsets{
                        {{0, 0, 0}, {mipWidth, mipHeight, 1}}
                };
                std::array<vk::Offset3D, 2> dstOffsets{
                        {{0, 0, 0}, {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1}}
                };

                vk::ImageBlit2 blit{
                        .srcSubresource =
                                vk::ImageSubresourceLayers{
                                                           .aspectMask = vk::ImageAspectFlagBits::eColor,
                                                           .mipLevel = i - 1,
                                                           .baseArrayLayer = 0,
                                                           .layerCount = 1,
                                                           },
                        .srcOffsets = srcOffsets,
                        .dstSubresource =
                                vk::ImageSubresourceLayers{
                                                           .aspectMask = vk::ImageAspectFlagBits::eColor,
                                                           .mipLevel = i,
                                                           .baseArrayLayer = 0,
                                                           .layerCount = 1
                                },
                        .dstOffsets = dstOffsets,
                };

                commandBuffer.blitImage2(
                        vk::BlitImageInfo2{
                                .srcImage = *image.getImage(),
                                .srcImageLayout = vk::ImageLayout::eTransferSrcOptimal,
                                .dstImage = *image.getImage(),
                                .dstImageLayout = vk::ImageLayout::eTransferDstOptimal,
                                .regionCount = 1,
                                .pRegions = &blit,
                                .filter = vk::Filter::eLinear, // TODO: fix!!
                        }
                );

                barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
                barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
                barrier.srcAccessMask = vk::AccessFlagBits2::eTransferRead,
                barrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
                barrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
                barrier.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader;

                commandBuffer.pipelineBarrier2(
                        vk::DependencyInfo{.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier}
                );

                if (mipWidth > 1) mipWidth /= 2;
                if (mipHeight > 1) mipHeight /= 2;
            }

            barrier.subresourceRange.baseMipLevel = mipLevels - 1;
            barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
            barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            barrier.srcAccessMask = vk::AccessFlagBits2::eTransferRead,
            barrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
            barrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
            barrier.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader;

            commandBuffer.pipelineBarrier2(
                    vk::DependencyInfo{.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier}
            );
        });

        return image;
    }

}
