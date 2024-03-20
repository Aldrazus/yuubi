#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

namespace yuubi {

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    inline bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

class DeviceSelector {
public:
    vk::PhysicalDevice selectPhysicalDevice();

    static const std::vector<const char*> deviceExtensions;
private:
    struct SwapChainSupportDetails {
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> presentModes;
    };

    bool isDeviceSuitable(vk::PhysicalDevice physicalDevice);
    QueueFamilyIndices findQueueFamilyIndices(vk::PhysicalDevice physicalDevice);
    bool supportsRequiredDeviceExtensions(vk::PhysicalDevice physicalDevice);
    SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice physicalDevice);

    // TODO: consider using Context class instead
    vk::Instance instance_;
    vk::SurfaceKHR surface_;
};

}
