#include "renderer/vulkan/device.h"

#include <vulkan/vulkan.hpp>

#include "renderer/vulkan/device_selector.h"
#include "renderer/vulkan/vma_usage.h"
#include "pch.h"

namespace yuubi {
Device::Device(Context* context) : context_(context) {

    VmaAllocatorCreateInfo allocInfo {
        .physicalDevice = physicalDevice_,
        .device = device_,
        .instance = context_->getInstance()
    };

    vmaCreateAllocator(&allocInfo, &allocator_);
}

Device::~Device() { device_.destroy(); }

void Device::selectPhysicalDevice() {
    std::vector<vk::PhysicalDevice> physicalDevices = context_->enumeratePhysicalDevices();

    auto suitableDeviceIter = std::find_if(
        physicalDevices.begin(), physicalDevices.end(),
        [this](vk::PhysicalDevice device) { return isDeviceSuitable(device); });

    if (suitableDeviceIter == physicalDevices.end()) {
        UB_ERROR("Failed to find a suitable GPU!");
    }

    physicalDevice_ = *suitableDeviceIter;
}

void Device::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);

    float priority = 1.0f;
    vk::DeviceQueueCreateInfo queueCreateInfo{
        .queueFamilyIndex = indices.graphicsFamily.value(),
        .queueCount = 1,
        .pQueuePriorities = &priority,
    };

    vk::PhysicalDeviceFeatures deviceFeatures{.samplerAnisotropy = vk::True};

    vk::DeviceCreateInfo createInfo{
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledLayerCount = 0,
        .enabledExtensionCount =
            static_cast<uint32_t>(deviceExtensions_.size()),
        .ppEnabledExtensionNames = deviceExtensions_.data(),
        .pEnabledFeatures = &deviceFeatures,
    };

    device_ = physicalDevice_.createDevice(createInfo);
    graphicsQueue_ = device_.getQueue(indices.graphicsFamily.value(), 0);
    presentQueue_ = device_.getQueue(indices.presentFamily.value(), 0);
}

QueueFamilyIndices Device::findQueueFamilies(vk::PhysicalDevice physicalDevice) const {
    QueueFamilyIndices indices;

    std::vector<vk::QueueFamilyProperties> properties =
        physicalDevice.getQueueFamilyProperties();

    for (const auto [i, p] : std::views::enumerate(properties)) {
        if (p.queueFlags & vk::QueueFlagBits::eGraphics &&
            context_->deviceSupportsSurface(physicalDevice, i)) {
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
        if (context_->deviceSupportsSurface(physicalDevice, i)) {
            indices.presentFamily = i;
        }
    }

    return indices;
}

bool Device::isDeviceSuitable(vk::PhysicalDevice physicalDevice) const
{
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    bool extensionsSupported = checkDeviceExtensionSupport(physicalDevice);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        auto swapChainSupport = context_->querySwapChainSupport(physicalDevice);
        swapChainAdequate = !swapChainSupport.formats.empty() &&
                            !swapChainSupport.presentModes.empty();
    }

    auto supportedFeatures = physicalDevice.getFeatures();

    return indices.isComplete() && extensionsSupported && swapChainAdequate &&
           supportedFeatures.samplerAnisotropy;
}
bool Device::checkDeviceExtensionSupport(vk::PhysicalDevice device) const
{
    auto extensionProperties = device.enumerateDeviceExtensionProperties();
    return std::all_of(
        deviceExtensions_.begin(), deviceExtensions_.end(),
        [&extensionProperties](const std::string& extension) {
            return std::find_if(extensionProperties.begin(),
                                extensionProperties.end(),
                                [&extension](const vk::ExtensionProperties& p) {
                                    return std::strcmp(p.extensionName.data(),
                                                       extension.data());
                                }) != extensionProperties.end();
        });
}

const std::vector<const char*> Device::deviceExtensions_ = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

}
