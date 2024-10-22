#include "renderer/device.h"
#include "renderer/vma/image.h"
#include "renderer/vma/allocator.h"
#include "renderer/vulkan/util.h"

namespace yuubi {

Image::Image(Allocator* allocator, ImageCreateInfo createInfo)
    : allocator_(allocator) {
    vk::ImageCreateInfo imageInfo{
        .imageType = vk::ImageType::e2D,
        .format = createInfo.format,
        .extent = {.width = createInfo.width, .height = createInfo.height, .depth = 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling = createInfo.tiling,
        .usage = createInfo.usage,
        .sharingMode = vk::SharingMode::eExclusive,
        .initialLayout = vk::ImageLayout::eUndefined,
    };

    VmaAllocationCreateInfo allocInfo{};

    VkImage vkImage;

    const VkImageCreateInfo legacyImageInfo = imageInfo;
    vmaCreateImage(allocator_->getAllocator(), &legacyImageInfo, &allocInfo, &vkImage, &allocation_, nullptr);
    image_ = vk::raii::Image{allocator_->getDevice(), vkImage};
}

Image::Image(Image&& rhs) noexcept
    : image_(std::exchange(rhs.image_, nullptr)),
      allocator_(std::exchange(rhs.allocator_, nullptr)),
      allocation_(std::exchange(rhs.allocation_, nullptr)) {}

Image& Image::operator=(Image&& rhs) noexcept {
    if (this != &rhs) {
        std::swap(allocator_, rhs.allocator_);
        std::swap(image_, rhs.image_);
        std::swap(allocation_, rhs.allocation_);
    }
    return *this;
}

Image::~Image() { destroy(); }

void Image::destroy() {
    if (allocator_ != nullptr) {
        vmaDestroyImage(
            allocator_->getAllocator(), image_.release(), allocation_
        );
    }
}

// PERF: Use staging buffer pool. Use dedicated transfer queue.
Image createImageFromData(Device& device, const ImageData& data, bool srgb) {
    // Create staging buffer.
    vk::DeviceSize imageSize = data.width * data.height * 4;

    vk::BufferCreateInfo stagingBufferCreateInfo{
        .size = imageSize,
        .usage = vk::BufferUsageFlagBits::eTransferSrc,
    };

    VmaAllocationCreateInfo stagingBufferAllocCreateInfo{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    Buffer stagingBuffer = device.createBuffer(
        stagingBufferCreateInfo, stagingBufferAllocCreateInfo
    );

    // Copy image data onto mapped memory in staging buffer.
    std::memcpy(
        stagingBuffer.getMappedMemory(), data.pixels,
        static_cast<size_t>(imageSize)
    );

    // Create image.
    Image image(
        &device.allocator(),
        ImageCreateInfo{
            .width = data.width,
            .height = data.height,
            .format = srgb ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = vk::ImageUsageFlagBits::eSampled |
                     vk::ImageUsageFlagBits::eTransferDst,
            .properties = vk::MemoryPropertyFlagBits::eDeviceLocal
        }
    );

    // Upload data to image.
    device.submitImmediateCommands(
        [&stagingBuffer, &image,
         &data](const vk::raii::CommandBuffer& commandBuffer) {
            transitionImage(
                commandBuffer, *image.getImage(), vk::ImageLayout::eUndefined,
                vk::ImageLayout::eTransferDstOptimal
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
                .imageExtent =
                    vk::Extent3D{
                                               .width = data.width, .height = data.height, .depth = 1
                    }
            };
            commandBuffer.copyBufferToImage(
                *stagingBuffer.getBuffer(), *image.getImage(),
                vk::ImageLayout::eTransferDstOptimal, {copyRegion}
            );

            transitionImage(
                commandBuffer, *image.getImage(),
                vk::ImageLayout::eTransferDstOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal
            );
        }
    );

    return image;
}

}
