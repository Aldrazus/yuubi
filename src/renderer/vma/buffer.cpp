#include "renderer/vma/buffer.h"
#include "renderer/vma/allocator.h"

namespace yuubi {

Buffer::Buffer(Allocator* allocator,
               const VkBufferCreateInfo& createInfo,
               const VmaAllocationCreateInfo& allocCreateInfo)
    : allocator_(allocator) {

    VkBuffer buffer;
    vmaCreateBuffer(allocator_->getAllocator(), &createInfo, &allocCreateInfo, &buffer, &allocation_, &allocationInfo_);
    buffer_ = vk::raii::Buffer(allocator_->getDevice(), buffer);

    if (createInfo.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
        address_ = allocator_->getDevice().getBufferAddress(vk::BufferDeviceAddressInfo{
            .buffer = *buffer_
        });
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
    }

    return *this;
}

Buffer::~Buffer() { destroy(); }

void Buffer::destroy() {
    if (allocator_ != nullptr) {
        vmaDestroyBuffer(allocator_->getAllocator(), buffer_.release(), allocation_);
    }
}

}
