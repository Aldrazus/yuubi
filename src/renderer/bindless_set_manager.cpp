#include "renderer/bindless_set_manager.h"
#include "renderer/descriptor_layout_builder.h"
#include "renderer/device.h"
#include "renderer/resources/texture.h"

namespace yuubi {

BindlessSetManager::BindlessSetManager(const std::shared_ptr<Device>& device) : device_(device) {
    // Create layout.
    DescriptorLayoutBuilder layoutBuilder(device_);

    vk::DescriptorBindingFlags bindingFlags =
        vk::DescriptorBindingFlagBits::ePartiallyBound |
        vk::DescriptorBindingFlagBits::eUpdateAfterBind;

    textureSetLayout_ =
        layoutBuilder
            .addBinding(vk::DescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .descriptorCount = 1024,
                .stageFlags = vk::ShaderStageFlagBits::eFragment,
            })
            .build(
                vk::DescriptorSetLayoutBindingFlagsCreateInfo{
                    .bindingCount = 1, .pBindingFlags = &bindingFlags},
                vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool);

    // Create pool.
    std::vector<vk::DescriptorPoolSize> poolSizes {
        vk::DescriptorPoolSize{ 
            .type = vk::DescriptorType::eStorageImage, 
            .descriptorCount = 1024
        },
        vk::DescriptorPoolSize{
            .type = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1024
        },
        vk::DescriptorPoolSize{
            .type = vk::DescriptorType::eSampler,
            .descriptorCount = 1024
        },
        vk::DescriptorPoolSize{
            .type = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = 1024
        }
    };

    vk::DescriptorPoolCreateInfo poolInfo {
        .flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind | vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = 1024,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };

    pool_ = device_->getDevice().createDescriptorPool(poolInfo);


    // Create set.
    vk::DescriptorSetAllocateInfo allocInfo {
        .descriptorPool = *pool_,
        .descriptorSetCount = 1,
        .pSetLayouts = &*textureSetLayout_
    };

    vk::raii::DescriptorSets sets(device_->getDevice(), allocInfo);
    textureSet_ = vk::raii::DescriptorSet(std::move(sets[0]));
}

uint32_t BindlessSetManager::addTexture(const Texture& texture)
{
    // TODO: Maintain a free list of IDs when handling texture unloading.
    static uint32_t id = 0;
    vk::DescriptorImageInfo imageInfo{
        .sampler = *texture.getSampler(),
        .imageView = *texture.getImageView(),
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
    };

    device_->getDevice().updateDescriptorSets({
        vk::WriteDescriptorSet{
            .dstSet = *textureSet_,
            .dstBinding = 0,
            .dstArrayElement = id,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .pImageInfo = &imageInfo
        }
    }, {});

    return id++;
}

}
