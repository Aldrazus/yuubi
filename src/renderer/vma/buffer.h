#pragma once

#include "core/util.h"
#include "renderer/vulkan_usage.h"
#include "pch.h"

namespace yuubi {

    class Allocator;
    class Device;

    class Buffer : NonCopyable {
    public:
        Buffer() = default;
        Buffer(Allocator* allocator, const vk::BufferCreateInfo& createInfo,
               const vma::AllocationCreateInfo& allocCreateInfo);
        Buffer(Buffer&& rhs) noexcept;
        Buffer& operator=(Buffer&& rhs) noexcept;
        ~Buffer();

        void upload(const Device& device, const void* data, size_t size, size_t offset) const;

        [[nodiscard]] const vk::raii::Buffer& getBuffer() const { return buffer_; }
        [[nodiscard]] void* getMappedMemory() const { return allocationInfo_.pMappedData; }
        [[nodiscard]] const vk::DeviceAddress& getAddress() const { return address_; }

    private:
        Allocator* allocator_ = nullptr;
        vk::raii::Buffer buffer_ = nullptr;
        vma::Allocation allocation_ = nullptr;
        vma::AllocationInfo allocationInfo_;
        vk::DeviceAddress address_;

        vk::raii::Buffer stagingBuffer_ = nullptr;
        vma::AllocationInfo stagingBufferAllocationInfo_;
        vma::Allocation stagingBufferAllocation_;
    };

}
