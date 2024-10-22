#include "renderer/resources/texture_manager.h"
#include "renderer/descriptor_layout_builder.h"
#include "renderer/device.h"
#include "glm/glm.hpp"
#include "renderer/vma/image.h"

namespace yuubi {

TextureManager::TextureManager(std::shared_ptr<Device> device)
    : device_(std::move(device)) {
    createDescriptorSet();
    createErrorTexture();
}

ResourceHandle TextureManager::addResource(
    const std::shared_ptr<Texture>& texture
) {
    auto handle = ResourceManager::addResource(texture);

    vk::DescriptorImageInfo imageInfo{
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
                                   .pImageInfo = &imageInfo}
    },
        {}
    );

    return handle;
}

void TextureManager::createDescriptorSet() {
    // Create layout.
    DescriptorLayoutBuilder layoutBuilder(device_);

    vk::DescriptorBindingFlags bindingFlags =
        vk::DescriptorBindingFlagBits::eVariableDescriptorCount |
        vk::DescriptorBindingFlagBits::ePartiallyBound |
        vk::DescriptorBindingFlagBits::eUpdateAfterBind;

    textureSetLayout_ =
        layoutBuilder
            .addBinding(vk::DescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .descriptorCount = maxTextures,
                .stageFlags = vk::ShaderStageFlagBits::eFragment,
            })
            .build(
                vk::DescriptorSetLayoutBindingFlagsCreateInfo{
                    .bindingCount = 1, .pBindingFlags = &bindingFlags
                },
                vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool
            );

    // Create pool.
    std::vector<vk::DescriptorPoolSize> poolSizes{
        vk::DescriptorPoolSize{
                               .type = vk::DescriptorType::eStorageImage,
                               .descriptorCount = maxTextures                                      },
        vk::DescriptorPoolSize{
                               .type = vk::DescriptorType::eUniformBuffer,
                               .descriptorCount = maxTextures                                      },
        vk::DescriptorPoolSize{
                               .type = vk::DescriptorType::eSampler, .descriptorCount = maxTextures},
        vk::DescriptorPoolSize{
                               .type = vk::DescriptorType::eCombinedImageSampler,
                               .descriptorCount = maxTextures                                      }
    };

    vk::DescriptorPoolCreateInfo poolInfo{
        .flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind |
                 vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = maxTextures,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };

    pool_ = device_->getDevice().createDescriptorPool(poolInfo);

    // Create set.
    vk::StructureChain<
        vk::DescriptorSetAllocateInfo,
        vk::DescriptorSetVariableDescriptorCountAllocateInfo>
        allocInfo{
            vk::DescriptorSetAllocateInfo{
                                          .descriptorPool = *pool_,
                                          .descriptorSetCount = 1,
                                          .pSetLayouts = &*textureSetLayout_
            },
            vk::DescriptorSetVariableDescriptorCountAllocateInfo{
                                          .descriptorSetCount = 1, .pDescriptorCounts = &maxTextures
            }
    };

    vk::raii::DescriptorSets sets(device_->getDevice(), allocInfo.get());
    textureSet_ = vk::raii::DescriptorSet(std::move(sets[0]));
}

void TextureManager::createErrorTexture() {
    // Magenta checkerboard image
    uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
    uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));

    const size_t textureWidth = 16;
    const size_t numChannels = 4;

    std::vector<unsigned char> pixels;
    pixels.reserve(textureWidth * textureWidth * numChannels);

    ImageData imageData{
        .pixels = pixels.data(),
        .width = textureWidth,
        .height = textureWidth,
        .numChannels = numChannels,
        .format = vk::Format::eR8G8B8A8Srgb
    };

    for (size_t rowIdx = 0; rowIdx < textureWidth; rowIdx++) {
        for (size_t colIdx = 0; colIdx < textureWidth; colIdx++) {
            auto color = ((colIdx % 2) ^ (rowIdx % 2)) ? magenta : black;

            for (size_t channelIdx = 0; channelIdx < numChannels;
                 channelIdx++) {
                const size_t byteIdx =
                    numChannels * (rowIdx * textureWidth + colIdx) + channelIdx;
                imageData.pixels[byteIdx] = (color >> (channelIdx * 8)) & 0xff;
            }
        }
    }

    auto errorCheckerboardImage = createImageFromData(*device_, imageData);
    auto errorCheckerboardView = device_->createImageView(
        *errorCheckerboardImage.getImage(), vk::Format::eR8G8B8A8Srgb,
        vk::ImageAspectFlagBits::eColor
    );

    // TODO: write createSampler() member function on Device
    auto errorCheckerboardSampler =
        device_->getDevice().createSampler(vk::SamplerCreateInfo{
            .magFilter = vk::Filter::eLinear,
            .minFilter = vk::Filter::eLinear,
            .mipmapMode = vk::SamplerMipmapMode::eLinear,
            .addressModeU = vk::SamplerAddressMode::eRepeat,
            .addressModeV = vk::SamplerAddressMode::eRepeat,
            .addressModeW = vk::SamplerAddressMode::eRepeat,
            .mipLodBias = 0.0f,
            .anisotropyEnable = vk::True,
            .maxAnisotropy = device_->getPhysicalDevice()
                                 .getProperties()
                                 .limits.maxSamplerAnisotropy,
            .compareEnable = vk::False,
            .compareOp = vk::CompareOp::eAlways,
            .minLod = 0.0f,
            .maxLod = 0.0f,
            .borderColor = vk::BorderColor::eIntOpaqueBlack,
            .unnormalizedCoordinates = vk::False,
        });

    auto errorCheckerboardTexture = std::make_shared<Texture>(
        std::move(errorCheckerboardImage), std::move(errorCheckerboardView),
        std::move(errorCheckerboardSampler)
    );
    addResource(errorCheckerboardTexture);
}

}
