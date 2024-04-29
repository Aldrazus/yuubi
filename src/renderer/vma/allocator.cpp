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

    UB_INFO("creating allocator");
    allocator_ = vma::createAllocator(allocatorInfo);
}

Allocator::Allocator(Allocator&& rhs)
{
    UB_INFO("moving alloc");
    allocator_ = rhs.allocator_;
    rhs.allocator_ = nullptr;
}
Allocator& Allocator::operator=(Allocator&& rhs)
{
    UB_INFO("move assigning alloc");
    allocator_ = rhs.allocator_;
    rhs.allocator_ = nullptr;
    return *this;
}

Allocator::~Allocator()
{
    UB_INFO("destroying allocator");
    allocator_.destroy();
}


}
