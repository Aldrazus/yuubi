#pragma once

#include "core/util.h"
#include "renderer/vulkan_usage.h"
#include "pch.h"

namespace yuubi {

class Allocator;

class Buffer : NonCopyable {
public:
    Buffer() {}
    Buffer(std::shared_ptr<Allocator> allocator,
           const VkBufferCreateInfo& createInfo,
           const VmaAllocationCreateInfo& allocCreateInfo);
    Buffer(Buffer&& rhs) = default;
    Buffer& operator=(Buffer&& rhs);
    ~Buffer();

    inline const vk::raii::Buffer& getBuffer() const { return buffer_; }
    inline void* getMappedMemory() { return allocationInfo_.pMappedData; }
private:
    void destroy();

    vk::raii::Buffer buffer_ = nullptr;
    std::shared_ptr<Allocator> allocator_ = nullptr;
    VmaAllocation allocation_;
    VmaAllocationInfo allocationInfo_;
};

}
