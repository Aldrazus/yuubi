#include "renderer/vma/allocator.h"

namespace yuubi {

Allocator::Allocator(const vk::raii::Instance& instance,
              const vk::raii::PhysicalDevice& physicalDevice,
              const vk::raii::Device& device) : device_(&device)
{
    VmaAllocatorCreateInfo allocatorInfo{
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = *physicalDevice,
        .device = *device,
        .instance = *instance,
        .vulkanApiVersion = vk::ApiVersion13,
    };

    vmaCreateAllocator(&allocatorInfo, &allocator_);
}

Allocator::Allocator(Allocator&& rhs)
{
    allocator_ = rhs.allocator_;
    device_ = rhs.device_;
    rhs.allocator_ = nullptr;
    rhs.device_ = nullptr;
}
Allocator& Allocator::operator=(Allocator&& rhs)
{
    allocator_ = rhs.allocator_;
    device_ = rhs.device_;
    rhs.allocator_ = nullptr;
    rhs.device_ = nullptr;
    
    return *this;
}

Allocator::~Allocator()
{
    vmaDestroyAllocator(allocator_);
}


}
