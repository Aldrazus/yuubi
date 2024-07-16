#pragma once

#include <print>
#include "core/util.h"
#include "renderer/vulkan_usage.h"

namespace yuubi {

// RAII wrapper over vma::Allocator
class Allocator : NonCopyable {
public:
    Allocator(const vk::raii::Instance& instance,
              const vk::raii::PhysicalDevice& physicalDevice,
              const vk::raii::Device& device);
    Allocator(std::nullptr_t) {};
    Allocator(Allocator&& rhs);
    Allocator& operator=(Allocator&& rhs);
    ~Allocator();
    
    inline VmaAllocator& getAllocator() { return allocator_; }
    const vk::raii::Device& getDevice() const { return *device_; }

private:
    VmaAllocator allocator_;
    // TODO: make shared
    const vk::raii::Device* device_;
};

}
