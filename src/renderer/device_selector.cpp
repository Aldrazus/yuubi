#include "device_selector.h"
#include "pch.h"

namespace yuubi {
DeviceSelector::DeviceSelector(vk::Instance instance) : instance_(instance) {}

DeviceSelector& DeviceSelector::preferGpuType(vk::PhysicalDeviceType type) {
    preferredGpuType_ = type;
    return *this;
}
DeviceSelector& DeviceSelector::requireApiVersion(uint32_t version) {
    apiVersion_ = version;
    return *this;
}

DeviceSelector& DeviceSelector::requiresSurfaceSupport(vk::SurfaceKHR surface)
{
    surface_ = surface;
    return *this;
}

DeviceSelector& DeviceSelector::requireFeatures(
    const vk::PhysicalDeviceFeatures& features) {
    requiredFeatures_.get<vk::PhysicalDeviceFeatures2>().features = features;
    return *this;
}

DeviceSelector& DeviceSelector::requireFeatures11(
    const vk::PhysicalDeviceVulkan11Features& features) {
    requiredFeatures_.get<vk::PhysicalDeviceVulkan11Features>() = features;
    return *this;
}

DeviceSelector& DeviceSelector::requireFeatures12(
    const vk::PhysicalDeviceVulkan12Features& features) {
    requiredFeatures_.get<vk::PhysicalDeviceVulkan12Features>() = features;
    return *this;
}

DeviceSelector& DeviceSelector::requireFeatures13(
    const vk::PhysicalDeviceVulkan13Features& features) {
    requiredFeatures_.get<vk::PhysicalDeviceVulkan13Features>() = features;
    return *this;
}

DeviceSelector& DeviceSelector::requiresExtensions(const std::vector<const char*>& extensions)
{
    requiredExtensions_ = extensions;
    return *this;
}
DeviceSelector& DeviceSelector::requireDedicatedTransferQueue() {
    shouldRequireDedicatedTransferQueue_ = true;
    return *this;
}

DeviceSelector& DeviceSelector::requireDedicatedComputeQueue() {
    shouldRequireDedicatedComputeQueue_ = true;
    return *this;
}

PhysicalDevice DeviceSelector::select() {
    auto physicalDevices = instance_.enumeratePhysicalDevices();

    std::partition(physicalDevices.begin(), physicalDevices.end(),
                   [this](vk::PhysicalDevice device) {
                       return device.getProperties().deviceType ==
                              preferredGpuType_;
                   });

    auto suitableDeviceIter = std::find_if(
        physicalDevices.begin(), physicalDevices.end(),
        [this](vk::PhysicalDevice device) { return isDeviceSuitable(device); });

    if (suitableDeviceIter == physicalDevices.end()) {
        UB_ERROR("Failed to find a suitable GPU!");
    }

    return PhysicalDevice{.physicalDevice = *suitableDeviceIter, .featuresToEnable = requiredFeatures_, .extensionsToEnable = requiredExtensions_};
}

bool DeviceSelector::isDeviceSuitable(vk::PhysicalDevice physicalDevice)
{
    bool isGraphicsCapable = false;
    std::vector<vk::QueueFamilyProperties> properties =
        physicalDevice.getQueueFamilyProperties();

    for (const auto [i, p] : std::views::enumerate(properties)) {
        if (p.queueFlags & vk::QueueFlagBits::eGraphics &&
            physicalDevice.getSurfaceSupportKHR(i, surface_)) {
            isGraphicsCapable = true;
            break;
        }
    }

    return isGraphicsCapable && supportsFeatures(physicalDevice);
}

bool DeviceSelector::supportsFeatures(vk::PhysicalDevice physicalDevice) {
    auto supportedFeatures = physicalDevice.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features, vk::PhysicalDeviceVulkan12Features, vk::PhysicalDeviceVulkan13Features>();

    // TODO: compare all features
    auto availableFeatures11 = supportedFeatures.get<vk::PhysicalDeviceVulkan11Features>();
    auto requiredFeatures11 = requiredFeatures_.get<vk::PhysicalDeviceVulkan11Features>();

    auto availableFeatures12 = supportedFeatures.get<vk::PhysicalDeviceVulkan12Features>();
    auto requiredFeatures12 = requiredFeatures_.get<vk::PhysicalDeviceVulkan12Features>();
    if (requiredFeatures12.bufferDeviceAddress && !availableFeatures12.bufferDeviceAddress) return false;
    if (requiredFeatures12.descriptorIndexing && !availableFeatures12.descriptorIndexing) return false;

    auto availableFeatures13 = supportedFeatures.get<vk::PhysicalDeviceVulkan13Features>();
    auto requiredFeatures13 = requiredFeatures_.get<vk::PhysicalDeviceVulkan13Features>();
    if (requiredFeatures13.dynamicRendering && !availableFeatures13.dynamicRendering) return false;
    if (requiredFeatures13.synchronization2 && !availableFeatures13.synchronization2) return false;

    return true;
}

}
