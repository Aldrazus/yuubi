#include "renderer/vulkan/device.h"

#include <vulkan/vulkan.hpp>

#include "renderer/vulkan/device_selector.h"
#include "renderer/vulkan/vma_usage.h"
#include "pch.h"

namespace yuubi {
Device::Device(vk::Instance instance, vk::PhysicalDevice physicaldevice, QueueFamilyIndices indices) : physicalDevice_(physicaldevice), queueFamilyIndices_(indices) {
    float priority = 1.0f;
    vk::DeviceQueueCreateInfo queueCreateInfo{
        .queueFamilyIndex = queueFamilyIndices_.graphicsFamily.value(),
        .queueCount = 1,
        .pQueuePriorities = &priority,
    };

    vk::PhysicalDeviceFeatures deviceFeatures{.samplerAnisotropy = vk::True};

    vk::DeviceCreateInfo createInfo{
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledLayerCount = 0,
        .enabledExtensionCount =
            static_cast<uint32_t>(DeviceSelector::deviceExtensions.size()),
        .ppEnabledExtensionNames = DeviceSelector::deviceExtensions.data(),
        .pEnabledFeatures = &deviceFeatures,
    };

    device_ = physicalDevice_.createDevice(createInfo);
    graphicsQueue_ =
        device_.getQueue(queueFamilyIndices_.graphicsFamily.value(), 0);
    presentQueue_ =
        device_.getQueue(queueFamilyIndices_.presentFamily.value(), 0);

    VmaAllocatorCreateInfo allocInfo {
        .physicalDevice = physicalDevice_,
        .device = device_,
        .instance = instance
    };

    vmaCreateAllocator(&allocInfo, &allocator_);
}

Device::~Device() { device_.destroy(); }
}
