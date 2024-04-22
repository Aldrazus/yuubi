#pragma once

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
    
    inline vma::Allocator& getAllocator() { return allocator_; }
private:
    vma::Allocator allocator_;
};

}
