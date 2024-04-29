#include "renderer/vma/image.h"
#include "renderer/vma/allocator.h"

namespace yuubi {

Image::Image(std::shared_ptr<Allocator> allocator, const vk::ImageCreateInfo& createInfo) : allocator_(allocator) {
    vma::AllocationCreateInfo allocInfo{};
    auto [image, allocation] = allocator_->getAllocator().createImage(createInfo, allocInfo);
    image_ = vk::raii::Image{allocator_->getDevice(), image};
    allocation_ = allocation;
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
        allocator_->getAllocator().destroyImage(image_.release(), allocation_);
    }
}

}
