#pragma once

#include "renderer/vulkan/device_selector.h"
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

#include "renderer/vulkan/vma_usage.h"

namespace yuubi {

class Device {
public:

    Device(vk::Instance instance, vk::PhysicalDevice physicaldevice, QueueFamilyIndices indices);
    ~Device();

private:
    QueueFamilyIndices queueFamilyIndices_;
    vk::PhysicalDevice physicalDevice_;
    vk::Device device_;
    VmaAllocator allocator_;
    vk::Queue graphicsQueue_;
    vk::Queue presentQueue_;
};

}
