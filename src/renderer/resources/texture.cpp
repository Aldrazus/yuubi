#include "renderer/resources/texture.h"

namespace yuubi {

Texture::Texture(Device& device, const std::string& name) {
    ImageData imageData = loadImage(name);
    vk::DeviceSize imageSize = imageData.width() * imageData.height() * 4;

    // Create staging buffer.
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
        stagingBuffer.getMappedMemory(), imageData.pixels(),
        static_cast<size_t>(imageSize)
    );

    image_ = Image(
        &device.allocator(),
        ImageCreateInfo{
            .width = static_cast<uint32_t>(imageData.width()),
            .height = static_cast<uint32_t>(imageData.height()),
            .format = vk::Format::eR8G8B8A8Srgb,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = vk::ImageUsageFlagBits::eSampled |
                     vk::ImageUsageFlagBits::eTransferDst,
            .properties = vk::MemoryPropertyFlagBits::eDeviceLocal
        }
    );

    device.submitImmediateCommands(
        [this, &stagingBuffer,
         &imageData](const vk::raii::CommandBuffer& commandBuffer) {
            transitionImage(
                commandBuffer, *image_.getImage(), vk::ImageLayout::eUndefined,
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
                                               .width = static_cast<uint32_t>(imageData.width()),
                                               .height = static_cast<uint32_t>(imageData.height()),
                                               .depth = 1
                    }
            };
            commandBuffer.copyBufferToImage(
                *stagingBuffer.getBuffer(), *image_.getImage(),
                vk::ImageLayout::eTransferDstOptimal, {copyRegion}
            );

            transitionImage(
                commandBuffer, *image_.getImage(),
                vk::ImageLayout::eTransferDstOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal
            );
        }
    );

    view_ = device.createImageView(
        *image_.getImage(), vk::Format::eR8G8B8A8Srgb,
        vk::ImageAspectFlagBits::eColor
    );

    // TODO: write createSampler() member function on Device
    sampler_ = device.getDevice().createSampler(vk::SamplerCreateInfo{
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .mipmapMode = vk::SamplerMipmapMode::eLinear,
        .addressModeU = vk::SamplerAddressMode::eRepeat,
        .addressModeV = vk::SamplerAddressMode::eRepeat,
        .addressModeW = vk::SamplerAddressMode::eRepeat,
        .mipLodBias = 0.0f,
        .anisotropyEnable = vk::True,
        .maxAnisotropy = device.getPhysicalDevice()
                             .getProperties()
                             .limits.maxSamplerAnisotropy,
        .compareEnable = vk::False,
        .compareOp = vk::CompareOp::eAlways,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = vk::BorderColor::eIntOpaqueBlack,
        .unnormalizedCoordinates = vk::False,
    });
}

Texture::Texture(Texture&& rhs)
    : name_(std::exchange(rhs.name_, {})),
      image_(std::exchange(rhs.image_, {})),
      view_(std::exchange(rhs.view_, nullptr)),
      sampler_(std::exchange(rhs.sampler_, nullptr)) {}

Texture& Texture::operator=(Texture&& rhs) {
    if (this != &rhs) {
        std::swap(name_, rhs.name_);
        std::swap(image_, rhs.image_);
        std::swap(view_, rhs.view_);
        std::swap(sampler_, rhs.sampler_);
    }

    return *this;
}

}
