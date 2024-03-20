#include "device_selector.h"

#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

namespace yuubi {

vk::PhysicalDevice selectPhysicalDevice(vk::Instance instance,
                                        vk::SurfaceKHR surface) {
    std::vector<vk::PhysicalDevice> physicalDevices =
        instance.enumeratePhysicalDevices();

    auto suitableDeviceIter = std::find_if(
        physicalDevices.begin(), physicalDevices.end(),
        [instance, surface](vk::PhysicalDevice device) { return isDeviceSuitable(instance, surface, device); });

    if (suitableDeviceIter == physicalDevices.end()) {
        UB_ERROR("Failed to find a suitable GPU!");
    }

    return *suitableDeviceIter;
}

static QueueFamilyIndices findQueueFamilyIndices(vk::SurfaceKHR surface, vk::PhysicalDevice physicalDevice) {
    QueueFamilyIndices indices;

    std::vector<vk::QueueFamilyProperties> properties =
        physicalDevice.getQueueFamilyProperties();

    for (const auto [i, p] : std::views::enumerate(properties)) {
        if (p.queueFlags & vk::QueueFlagBits::eGraphics &&
            physicalDevice.getSurfaceSupportKHR(i, surface)) {
            indices.graphicsFamily = indices.presentFamily = i;
        }
    }

    if (indices.isComplete()) {
        return indices;
    }

    for (const auto [i, p] : std::views::enumerate(properties)) {
        if (p.queueFlags & vk::QueueFlagBits::eGraphics) {
            indices.graphicsFamily = i;
        }
    }

    for (const auto [i, p] : std::views::enumerate(properties)) {
        if (physicalDevice.getSurfaceSupportKHR(i, surface)) {
            indices.presentFamily = i;
        }
    }

    return indices;
}

static bool isDeviceSuitable(vk::Instance instance, vk::SurfaceKHR surface,
                             vk::PhysicalDevice physicalDevice) {
    QueueFamilyIndices indices = findQueueFamilyIndices(surface, physicalDevice);
    bool extensionsSupported = supportsRequiredDeviceExtensions(physicalDevice);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        auto swapChainSupport = querySwapChainSupport(physicalDevice);
        swapChainAdequate = !swapChainSupport.formats.empty() &&
                            !swapChainSupport.presentModes.empty();
    }

    auto supportedFeatures = physicalDevice.getFeatures();

    return indices.isComplete() && extensionsSupported && swapChainAdequate &&
           supportedFeatures.samplerAnisotropy;
}


bool DeviceSelector::supportsRequiredDeviceExtensions(
    vk::PhysicalDevice physicalDevice) {
    auto extensionProperties =
        physicalDevice.enumerateDeviceExtensionProperties();
    return std::all_of(
        deviceExtensions.begin(), deviceExtensions.end(),
        [&extensionProperties](const std::string& extension) {
            return std::find_if(extensionProperties.begin(),
                                extensionProperties.end(),
                                [&extension](const vk::ExtensionProperties& p) {
                                    return std::strcmp(p.extensionName.data(),
                                                       extension.data());
                                }) != extensionProperties.end();
        });
}

DeviceSelector::SwapChainSupportDetails DeviceSelector::querySwapChainSupport(
    vk::PhysicalDevice physicalDevice) {
    return {.capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface_),
            .formats = physicalDevice.getSurfaceFormatsKHR(surface_),
            .presentModes = physicalDevice.getSurfacePresentModesKHR(surface_)};
}

const std::vector<const char*> DeviceSelector::deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};
}
