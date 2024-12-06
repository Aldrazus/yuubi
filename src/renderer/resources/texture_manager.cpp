#include "renderer/resources/texture_manager.h"
#include "renderer/descriptor_layout_builder.h"
#include "renderer/device.h"
#include "glm/glm.hpp"
#include "renderer/vma/image.h"

namespace yuubi {

    TextureManager::TextureManager(
            std::shared_ptr<Device> device, std::shared_ptr<vk::raii::DescriptorSet> textureSet
    ) : device_(std::move(device)), textureSet_(std::move(textureSet)) {
        createErrorTexture();
    }

    ResourceHandle TextureManager::addResource(const std::shared_ptr<Texture>& texture) {
        const auto handle = ResourceManager::addResource(texture);

        const vk::DescriptorImageInfo imageInfo{
                .sampler = *texture->sampler,
                .imageView = *texture->imageView,
                .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
        };

        device_->getDevice().updateDescriptorSets(
                {
                        vk::WriteDescriptorSet{
                                               .dstSet = *textureSet_,
                                               .dstBinding = 0,
                                               .dstArrayElement = handle,
                                               .descriptorCount = 1,
                                               .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                                               .pImageInfo = &imageInfo
                        }
        },
                {}
        );

        return handle;
    }

    void TextureManager::createErrorTexture() {
        // Magenta checkerboard image
        const uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
        const uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));

        constexpr size_t textureWidth = 16;
        constexpr size_t numChannels = 4;

        std::vector<unsigned char> pixels;
        pixels.reserve(textureWidth * textureWidth * numChannels);

        const ImageData imageData{
                .pixels = pixels.data(),
                .width = textureWidth,
                .height = textureWidth,
                .numChannels = numChannels,
                .format = vk::Format::eR8G8B8A8Srgb
        };

        for (size_t rowIdx = 0; rowIdx < textureWidth; rowIdx++) {
            for (size_t colIdx = 0; colIdx < textureWidth; colIdx++) {
                const auto color = ((colIdx % 2) ^ (rowIdx % 2)) ? magenta : black;

                for (size_t channelIdx = 0; channelIdx < numChannels; channelIdx++) {
                    const size_t byteIdx = numChannels * (rowIdx * textureWidth + colIdx) + channelIdx;
                    imageData.pixels[byteIdx] = (color >> (channelIdx * 8)) & 0xff;
                }
            }
        }

        auto errorCheckerboardImage = createImageFromData(*device_, imageData);
        auto errorCheckerboardView = device_->createImageView(
                *errorCheckerboardImage.getImage(), vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor
        );

        // TODO: write createSampler() member function on Device
        auto errorCheckerboardSampler = device_->getDevice().createSampler(
                vk::SamplerCreateInfo{
                        .magFilter = vk::Filter::eLinear,
                        .minFilter = vk::Filter::eLinear,
                        .mipmapMode = vk::SamplerMipmapMode::eLinear,
                        .addressModeU = vk::SamplerAddressMode::eRepeat,
                        .addressModeV = vk::SamplerAddressMode::eRepeat,
                        .addressModeW = vk::SamplerAddressMode::eRepeat,
                        .mipLodBias = 0.0f,
                        .anisotropyEnable = vk::True,
                        .maxAnisotropy = device_->getPhysicalDevice().getProperties().limits.maxSamplerAnisotropy,
                        .compareEnable = vk::False,
                        .compareOp = vk::CompareOp::eAlways,
                        .minLod = 0.0f,
                        .maxLod = 0.0f,
                        .borderColor = vk::BorderColor::eIntOpaqueBlack,
                        .unnormalizedCoordinates = vk::False,
                }
        );

        const auto errorCheckerboardTexture = std::make_shared<Texture>(
                std::move(errorCheckerboardImage), std::move(errorCheckerboardView), std::move(errorCheckerboardSampler)
        );
        addResource(errorCheckerboardTexture);
    }

}
