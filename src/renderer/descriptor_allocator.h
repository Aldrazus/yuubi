#pragma once

#include "core/util.h"
#include "renderer/vulkan_usage.h"
#include "pch.h"

namespace yuubi {

class Device;
class DescriptorAllocator : NonCopyable {
public:
    DescriptorAllocator() {}
    DescriptorAllocator(const std::shared_ptr<Device>& device);
    DescriptorAllocator(DescriptorAllocator&& other) = default;
    DescriptorAllocator& operator=(DescriptorAllocator&& rhs);
    void clear();
    vk::raii::DescriptorSet allocate(const vk::raii::DescriptorSetLayout& layout);
private:
    std::shared_ptr<Device> device_;
    vk::raii::DescriptorPool pool_ = nullptr;
};

}
