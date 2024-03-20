#include "device_selector.h"

namespace yuubi {

bool DeviceSelector::isDeviceSuitable(vk::PhysicalDevice physicalDevice) {
    QueueFamilyIndices indices = findQueueFamilyIndices(physicalDevice);
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

QueueFamilyIndices DeviceSelector::findQueueFamilyIndices(
    vk::PhysicalDevice physicalDevice) {
    QueueFamilyIndices indices;

    std::vector<vk::QueueFamilyProperties> properties =
        physicalDevice.getQueueFamilyProperties();

    for (const auto [i, p] : std::views::enumerate(properties)) {
        if (p.queueFlags & vk::QueueFlagBits::eGraphics &&
            physicalDevice.getSurfaceSupportKHR(i, surface_)) {
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
        if (physicalDevice.getSurfaceSupportKHR(i, surface_)) {
            indices.presentFamily = i;
        }
    }

    return indices;
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
