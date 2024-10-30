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
    uint32_t mipLevels = 1;
    uint32_t arrayLayers = 1;
};

struct ImageData {
    // TODO: use std::byte?
    unsigned char* pixels;
    uint32_t width;
    uint32_t height;
    uint32_t numChannels;
    vk::Format format;
};

class Image : NonCopyable {
public:
    Image() = default;
    Image(Allocator* allocator, ImageCreateInfo createInfo);
    Image(Image&& rhs) noexcept;
    Image& operator=(Image&& rhs) noexcept;
    ~Image();

    [[nodiscard]] inline const vk::raii::Image& getImage() const { return image_; }
    [[nodiscard]] inline vk::Format getImageFormat() const { return format_; }
    [[nodiscard]] inline uint32_t getMipLevels() const { return mipLevels_; }

private:
    void destroy();

    vk::raii::Image image_ = nullptr;
    vk::Format format_;
    uint32_t mipLevels_;

    // WARNING: raw pointer used here. This *should* be safe, assuming
    // all images are managed by the ImageManager. Any images created
    // outside of the context of the ImageManager may live beyond the
    // lifetime of the Allocator/Device that created it, potentially
    // causing a memory leak or crash.
    Allocator* allocator_ = nullptr;
    VmaAllocation allocation_;
};

class Device;
Image createImageFromData(Device& device, const ImageData& data);
}
