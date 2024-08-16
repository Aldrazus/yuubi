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
}


Buffer& Buffer::operator=(Buffer&& rhs) {
    // If rhs is this, no-op.
    if (this == &rhs) {
        return *this;
    }

    // Destroy this.
    destroy();

    // Move from rhs to this.
    buffer_ = std::move(rhs.buffer_);
    allocator_ = rhs.allocator_;
    allocation_ = rhs.allocation_;
    allocationInfo_ = rhs.allocationInfo_;

    // Invalidate rhs.
    rhs.buffer_ = nullptr;
    rhs.allocator_ = nullptr;
    rhs.allocation_ = nullptr;
    rhs.allocationInfo_ = VmaAllocationInfo{};

    return *this;
}

Buffer::~Buffer() { destroy(); }

void Buffer::destroy() {
    if (allocator_ != nullptr) {
        vmaDestroyBuffer(allocator_->getAllocator(), buffer_.release(), allocation_);
    }
}

}
