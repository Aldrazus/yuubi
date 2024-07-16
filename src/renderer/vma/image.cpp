#include "renderer/vma/image.h"
#include "renderer/vma/allocator.h"

namespace yuubi {

Image::Image(std::shared_ptr<Allocator> allocator, const VkImageCreateInfo& createInfo) : allocator_(allocator) {
    VmaAllocationCreateInfo allocInfo{};

    VkImage image;
    vmaCreateImage(allocator->getAllocator(), &createInfo, &allocInfo, &image, &allocation_, nullptr);
    image_ = vk::raii::Image{allocator_->getDevice(), image};
}

Image& Image::operator=(Image&& rhs) {
    if (this == &rhs) {
        return *this;  
    }
    destroy();
    image_ = std::move(rhs.image_);
    rhs.image_ = nullptr;
    allocator_ = rhs.allocator_;
    rhs.allocator_ = nullptr;
    allocation_ = rhs.allocation_;
    rhs.allocation_ = nullptr;
    return *this;
}

Image::~Image() {
    destroy();
}

void Image::destroy() {
    if (allocator_ != nullptr) {
        vmaDestroyImage(allocator_->getAllocator(), image_.release(), allocation_);
    }
}

}
