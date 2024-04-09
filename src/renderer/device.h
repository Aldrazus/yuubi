#pragma once

#include "vkutils/vulkan_usage.h"
#include "renderer/device_selector.h"

namespace yuubi {
struct Queue {
    vk::Queue queue;
    uint32_t familyIndex;
};

struct Buffer {
    vk::Buffer buffer;
    vma::Allocation allocation;
};

struct Device {
    Device(vk::Instance instance, const PhysicalDevice& physicalDevice);
    void destroy();
    Buffer createBuffer(size_t size, vk::BufferUsageFlags usage, vma::MemoryUsage memoryUsage);

    vk::Instance instance_;
    vk::Device device_;
    vk::PhysicalDevice physicalDevice_;
    vma::Allocator allocator_;
    vk::DispatchLoaderDynamic dld_;
    Queue graphicsQueue_;
};
}
