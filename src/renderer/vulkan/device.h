#pragma once

#include "renderer/vulkan/device_selector.h"
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

#include "renderer/vulkan/vma_usage.h"

#include "renderer/vulkan/context.h"

namespace yuubi {

class Context;

class Device {
public:
    Device(Context* context);
    ~Device();

private:
    struct SwapChainSupportDetails {
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> presentModes;
    };
    void selectPhysicalDevice();
    void createLogicalDevice();
    QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice physicalDevice) const;
    bool isDeviceSuitable(vk::PhysicalDevice physicalDevice) const;
    bool checkDeviceExtensionSupport(vk::PhysicalDevice physicalDevice) const;

    Context* context_;
    QueueFamilyIndices queueFamilyIndices_;
    vk::PhysicalDevice physicalDevice_;
    vk::Device device_;
    VmaAllocator allocator_;
    vk::Queue graphicsQueue_;
    vk::Queue presentQueue_;

    static const std::vector<const char*> deviceExtensions_;
};

}
