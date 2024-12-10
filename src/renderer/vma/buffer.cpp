#include "renderer/vma/buffer.h"
#include "renderer/vma/allocator.h"
#include "renderer/device.h"

namespace yuubi {

    Buffer::Buffer(
            Allocator* allocator, const vk::BufferCreateInfo& createInfo,
            const vma::AllocationCreateInfo& allocCreateInfo
    ) : allocator_(allocator) {
        // Create GPU buffer.
        auto [buffer, allocation] =
                allocator_->getAllocator().createBuffer(createInfo, allocCreateInfo, allocationInfo_);

        buffer_ = vk::raii::Buffer(allocator_->getDevice(), buffer);
        allocation_ = allocation;

        // Create staging buffer.
        vk::BufferCreateInfo stagingBufferCreateInfo{
                .pNext = nullptr, .size = createInfo.size, .usage = vk::BufferUsageFlagBits::eTransferSrc
        };

        vma::AllocationCreateInfo stagingBufferAllocCreateInfo{
                .flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite |
                         vma::AllocationCreateFlagBits::eMapped,
                .usage = vma::MemoryUsage::eAuto
        };

        auto [stagingBuffer, stagingAllocation] = allocator_->getAllocator().createBuffer(
                stagingBufferCreateInfo, stagingBufferAllocCreateInfo, stagingBufferAllocationInfo_
        );

        stagingBuffer_ = vk::raii::Buffer(allocator_->getDevice(), stagingBuffer);
        stagingBufferAllocation_ = stagingAllocation;

        if (createInfo.usage & vk::BufferUsageFlagBits::eShaderDeviceAddress) {
            address_ = allocator_->getDevice().getBufferAddress(vk::BufferDeviceAddressInfo{.buffer = *buffer_});
        }
    }

    Buffer& Buffer::operator=(Buffer&& rhs) noexcept {
        // If rhs is this, no-op.
        if (this != &rhs) {
            std::swap(buffer_, rhs.buffer_);
            std::swap(allocator_, rhs.allocator_);
            std::swap(allocation_, rhs.allocation_);
            std::swap(allocationInfo_, rhs.allocationInfo_);
            std::swap(address_, rhs.address_);
            std::swap(stagingBuffer_, rhs.stagingBuffer_);
            std::swap(stagingBufferAllocation_, rhs.stagingBufferAllocation_);
            std::swap(stagingBufferAllocationInfo_, rhs.stagingBufferAllocationInfo_);
        }

        return *this;
    }

    Buffer::~Buffer() {
        if (allocator_ != nullptr) {
            allocator_->getAllocator().destroyBuffer(buffer_.release(), allocation_);
            allocator_->getAllocator().destroyBuffer(stagingBuffer_.release(), stagingBufferAllocation_);
        }
    }

    // TODO: don't rely on immediate commands.
    void Buffer::upload(const Device& device, const void* data, size_t size, size_t offset) const {
        auto* mappedDataBytes = static_cast<std::byte*>(stagingBufferAllocationInfo_.pMappedData);
        std::memcpy(&mappedDataBytes[offset], data, size);

        device.submitImmediateCommands([this, size, offset](const vk::raii::CommandBuffer& commandBuffer) {
            vk::BufferCopy copyRegion{.srcOffset = offset, .dstOffset = offset, .size = size};
            commandBuffer.copyBuffer(*stagingBuffer_, *buffer_, {copyRegion});
        });
    }

}
