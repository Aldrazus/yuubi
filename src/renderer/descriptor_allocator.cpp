#include "renderer/descriptor_allocator.h"
#include "renderer/device.h"
#include "pch.h"

namespace yuubi {

DescriptorAllocator::DescriptorAllocator(const std::shared_ptr<Device>& device) : device_(device)
{
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
        }
    };

    vk::DescriptorPoolCreateInfo poolInfo {
        .flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind | vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = 1024,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };

    pool_ = device_->getDevice().createDescriptorPool(poolInfo);
}

DescriptorAllocator& DescriptorAllocator::operator=(DescriptorAllocator&& rhs)
{
    if (this == &rhs) {
        return *this;
    }

    // TODO: EXPLORE std::swap INSTEAD OF std::move 

    // Destroy this (done automatically)

    // Move rhs to this
    device_ = rhs.device_;
    pool_ = std::move(rhs.pool_);
    
    // Invalidate rhs
    rhs.device_ = nullptr;
    rhs.pool_ = nullptr;

    return *this;
}

void DescriptorAllocator::clear()
{
    pool_.reset();
}

vk::raii::DescriptorSet DescriptorAllocator::allocate(const vk::raii::DescriptorSetLayout& layout)
{
    vk::DescriptorSetAllocateInfo allocInfo {
        .descriptorPool = *pool_,
        .descriptorSetCount = 1,
        .pSetLayouts = &*layout
    };

    vk::raii::DescriptorSets sets(device_->getDevice(), allocInfo);
    return vk::raii::DescriptorSet(std::move(sets[0]));
}

}
