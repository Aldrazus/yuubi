#pragma once

#include "core/util.h"
#include "renderer/vulkan_usage.h"
#include "pch.h"

namespace yuubi {

class Allocator;

class Buffer : NonCopyable {
public:
    Buffer() = default;
    Buffer(Allocator* allocator,
           const VkBufferCreateInfo& createInfo,
           const VmaAllocationCreateInfo& allocCreateInfo);
    Buffer(Buffer&& rhs) = default;
    Buffer& operator=(Buffer&& rhs) noexcept;
    ~Buffer();

    [[nodiscard]] inline const vk::raii::Buffer& getBuffer() const { return buffer_; }
    [[nodiscard]] inline void* getMappedMemory() const { return allocationInfo_.pMappedData; }
    [[nodiscard]] inline const vk::DeviceAddress& getAddress() const { return address_; }
private:
    void destroy();

    vk::raii::Buffer buffer_ = nullptr;
    Allocator* allocator_ = nullptr;
    VmaAllocation allocation_;
    VmaAllocationInfo allocationInfo_;
    vk::DeviceAddress address_;
};

}
