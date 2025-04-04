#include "renderer/vma/allocator.h"

namespace yuubi {

    Allocator::Allocator(
        const vk::raii::Instance& instance, const vk::raii::PhysicalDevice& physicalDevice,
        const vk::raii::Device& device
    ) : device_(&device) {
        vma::AllocatorCreateInfo allocatorInfo{
            .flags = vma::AllocatorCreateFlagBits::eBufferDeviceAddress,
            .physicalDevice = physicalDevice,
            .device = device,
            .instance = instance,
            .vulkanApiVersion = vk::ApiVersion13
        };

        allocator_ = vma::createAllocator(allocatorInfo);
    }

    Allocator::Allocator(Allocator&& rhs) noexcept {
        allocator_ = rhs.allocator_;
        device_ = rhs.device_;
        rhs.allocator_ = nullptr;
        rhs.device_ = nullptr;
    }
    Allocator& Allocator::operator=(Allocator&& rhs) noexcept {
        allocator_ = rhs.allocator_;
        device_ = rhs.device_;
        rhs.allocator_ = nullptr;
        rhs.device_ = nullptr;

        return *this;
    }

    Allocator::~Allocator() { allocator_.destroy(); }


}
