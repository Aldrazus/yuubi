#include "renderer/vma/buffer.h"
#include "renderer/vma/allocator.h"
#include "renderer/device.h"

namespace yuubi {

    Buffer::Buffer(
        Allocator* allocator, const vk::BufferCreateInfo& createInfo, const VmaAllocationCreateInfo& allocCreateInfo
    ) : allocator_(allocator) {
        // Create GPU buffer.
        VkBuffer buffer;
        vmaCreateBuffer(
            allocator_->getAllocator(), reinterpret_cast<const VkBufferCreateInfo*>(&createInfo), &allocCreateInfo,
            &buffer, &allocation_, &allocationInfo_
        );
        buffer_ = vk::raii::Buffer(allocator_->getDevice(), buffer);

        // Create staging buffer.
        vk::BufferCreateInfo stagingBufferCreateInfo{
            .pNext = nullptr, .size = createInfo.size, .usage = vk::BufferUsageFlagBits::eTransferSrc
        };

        VmaAllocationCreateInfo stagingBufferAllocCreateInfo{
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO
        };

        VkBuffer stagingBuffer;
        vmaCreateBuffer(
            allocator_->getAllocator(), reinterpret_cast<const VkBufferCreateInfo*>(&stagingBufferCreateInfo),
            &stagingBufferAllocCreateInfo, &stagingBuffer, &stagingBufferAllocation_, &stagingBufferAllocationInfo_
        );
        stagingBuffer_ = vk::raii::Buffer(allocator_->getDevice(), stagingBuffer);

        if (createInfo.usage & vk::BufferUsageFlagBits::eShaderDeviceAddress) {
            address_ = allocator_->getDevice().getBufferAddress(vk::BufferDeviceAddressInfo{.buffer = *buffer_});
        }
    }

    Buffer::Buffer(Buffer&& rhs) noexcept :
        allocator_(std::exchange(rhs.allocator_, nullptr)), buffer_(std::exchange(rhs.buffer_, nullptr)),
        allocation_(std::exchange(rhs.allocation_, nullptr)), allocationInfo_(std::exchange(rhs.allocationInfo_, {})),
        address_(std::exchange(rhs.address_, {})), stagingBuffer_(std::exchange(rhs.stagingBuffer_, nullptr)),
        stagingBufferAllocationInfo_(std::exchange(rhs.stagingBufferAllocationInfo_, {})),
        stagingBufferAllocation_(std::exchange(rhs.stagingBufferAllocation_, nullptr)) {}

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
            vmaDestroyBuffer(allocator_->getAllocator(), buffer_.release(), allocation_);
            vmaDestroyBuffer(allocator_->getAllocator(), stagingBuffer_.release(), stagingBufferAllocation_);
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
