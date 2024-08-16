#include "renderer/device.h"
#include "renderer/vma/image.h"
#include "renderer/vma/allocator.h"

namespace yuubi {

Image::Image(Allocator* allocator, ImageCreateInfo createInfo)
    : allocator_(allocator) {
    vk::ImageCreateInfo imageInfo{
        .imageType = vk::ImageType::e2D,
        .format = createInfo.format,
        .extent = {.width = createInfo.width, .height = createInfo.height, .depth = 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling = createInfo.tiling,
        .usage = createInfo.usage,
        .sharingMode = vk::SharingMode::eExclusive,
        .initialLayout = vk::ImageLayout::eUndefined,
    };

    VmaAllocationCreateInfo allocInfo{};

    VkImage vkImage;

    const VkImageCreateInfo legacyImageInfo = imageInfo;
    vmaCreateImage(allocator_->getAllocator(), &legacyImageInfo, &allocInfo, &vkImage, &allocation_, nullptr);
    image_ = vk::raii::Image{allocator_->getDevice(), vkImage};
}

Image::Image(Image&& rhs)
    : image_(std::exchange(rhs.image_, nullptr)),
      allocator_(std::exchange(rhs.allocator_, nullptr)),
      allocation_(std::exchange(rhs.allocation_, nullptr)) {}

Image& Image::operator=(Image&& rhs) {
    if (this != &rhs) {
        std::swap(allocator_, rhs.allocator_);
        std::swap(image_, rhs.image_);
        std::swap(allocation_, rhs.allocation_);
    }
    return *this;
}

Image::~Image() { destroy(); }

void Image::destroy() {
    if (allocator_ != nullptr) {
        vmaDestroyImage(
            allocator_->getAllocator(), image_.release(), allocation_
        );
    }
}

}
