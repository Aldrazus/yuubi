#pragma once

#include "core/util.h"
#include "renderer/vulkan_usage.h"
#include "pch.h"

namespace yuubi {

class Allocator;

class Image : NonCopyable {
public:
    Image() {};
    Image(std::shared_ptr<Allocator> allocator, const vk::ImageCreateInfo& createInfo);
    Image(Image&& rhs) = default;
    Image& operator=(Image&& rhs);
    ~Image();

private:
    void destroy();

    vk::raii::Image image_ = nullptr;
    std::shared_ptr<Allocator> allocator_ = nullptr;
    vma::Allocation allocation_;
};
}
