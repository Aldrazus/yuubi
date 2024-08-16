#pragma once

#include "core/util.h"
#include "renderer/vulkan_usage.h"
#include "pch.h"

namespace yuubi {

class Allocator;

struct ImageCreateInfo {
    uint32_t width;
    uint32_t height;
    vk::Format format;
    vk::ImageTiling tiling;
    vk::ImageUsageFlags usage;
    vk::MemoryPropertyFlags properties;
};

class Image : NonCopyable {
public:
    Image(){};
    Image(Allocator* allocator, ImageCreateInfo createInfo);
    Image(Image&& rhs);
    Image& operator=(Image&& rhs);
    ~Image();

    inline const vk::raii::Image& getImage() const { return image_; }

private:
    void destroy();

    vk::raii::Image image_ = nullptr;

    // WARNING: raw pointer used here. This *should* be safe, assuming
    // all images are managed by the ImageManager. Any images created
    // outside of the context of the ImageManager may live beyond the
    // lifetime of the Allocator/Device that created it, potentially
    // causing a memory leak or crash.
    Allocator* allocator_ = nullptr;
    VmaAllocation allocation_;
};
}
