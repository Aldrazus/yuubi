#include "renderer/vma/allocator.h"

namespace yuubi {

Allocator::Allocator(const vk::raii::Instance& instance,
              const vk::raii::PhysicalDevice& physicalDevice,
              const vk::raii::Device& device) : device_(&device)
{
    vma::AllocatorCreateInfo allocatorInfo{
        .physicalDevice = physicalDevice,
        .device = device,
        .instance = instance,
        .vulkanApiVersion = vk::ApiVersion13
    };

    allocator_ = vma::createAllocator(allocatorInfo);
}

Allocator::Allocator(Allocator&& rhs)
{
    allocator_ = rhs.allocator_;
    rhs.allocator_ = nullptr;
}
Allocator& Allocator::operator=(Allocator&& rhs)
{
    allocator_ = rhs.allocator_;
    rhs.allocator_ = nullptr;
    return *this;
}

Allocator::~Allocator()
{
    allocator_.destroy();
}


}
