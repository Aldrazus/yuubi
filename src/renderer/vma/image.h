#pragma once

#include "core/util.h"
#include "renderer/vulkan_usage.h"
#include "pch.h"

namespace yuubi {

class Allocator;

class Image : NonCopyable {
public:
    Image() {};
    Image(std::shared_ptr<Allocator> allocator, const VkImageCreateInfo& createInfo);
    Image(Image&& rhs) = default;
    Image& operator=(Image&& rhs);
    ~Image();

    inline const vk::raii::Image& getImage() const { return image_; }

private:
    void destroy();

    vk::raii::Image image_ = nullptr;
    std::shared_ptr<Allocator> allocator_ = nullptr;
    VmaAllocation allocation_;
};
}
