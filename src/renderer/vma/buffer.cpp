#include "renderer/vma/buffer.h"
#include "renderer/vma/allocator.h"

namespace yuubi {

Buffer::Buffer(std::shared_ptr<Allocator> allocator,
               const vk::BufferCreateInfo& createInfo,
               const vma::AllocationCreateInfo& allocCreateInfo)
    : allocator_(allocator) {
    auto [buffer, allocation] =
        allocator_->getAllocator().createBuffer(createInfo, allocCreateInfo, allocationInfo_);
    buffer_ = vk::raii::Buffer(allocator_->getDevice(), buffer);
    allocation_ = allocation;
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
    rhs.allocationInfo_ = vma::AllocationInfo{};

    return *this;
}

Buffer::~Buffer() { destroy(); }

void Buffer::destroy() {
    if (allocator_ != nullptr) {
        allocator_->getAllocator().destroyBuffer(buffer_.release(),
                                                 allocation_);
    }
}

}
