#pragma once

#include <initializer_list>
#include "renderer/vulkan_usage.h"

namespace yuubi {
using DeviceFeatures = vk::StructureChain<
    vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
    vk::PhysicalDeviceVulkan12Features, vk::PhysicalDeviceVulkan13Features>;

struct PhysicalDevice {
    vk::PhysicalDevice physicalDevice;
    DeviceFeatures featuresToEnable;
    std::vector<const char*> extensionsToEnable;
};

class DeviceSelector {
public:
    DeviceSelector(vk::Instance instance);
    DeviceSelector& preferGpuType(vk::PhysicalDeviceType type);
    DeviceSelector& requiresSurfaceSupport(vk::SurfaceKHR surface);
    DeviceSelector& requireApiVersion(uint32_t version);
    DeviceSelector& requireFeatures(const vk::PhysicalDeviceFeatures& features);
    DeviceSelector& requireFeatures11(
        const vk::PhysicalDeviceVulkan11Features& features);
    DeviceSelector& requireFeatures12(
        const vk::PhysicalDeviceVulkan12Features& features);
    DeviceSelector& requireFeatures13(
        const vk::PhysicalDeviceVulkan13Features& features);
    DeviceSelector& requiresExtensions(const std::vector<const char*>& extensions);
    DeviceSelector& requireDedicatedTransferQueue();
    DeviceSelector& requireDedicatedComputeQueue();
    PhysicalDevice select();

private:
    bool isDeviceSuitable(vk::PhysicalDevice physicalDevice);
    bool supportsFeatures(vk::PhysicalDevice physicalDevice);
    vk::Instance instance_;
    vk::SurfaceKHR surface_;
    vk::PhysicalDeviceType preferredGpuType_ =
        vk::PhysicalDeviceType::eDiscreteGpu;
    uint32_t apiVersion_ = vk::ApiVersion10;
    vk::StructureChain<
        vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
        vk::PhysicalDeviceVulkan12Features, vk::PhysicalDeviceVulkan13Features>
        requiredFeatures_;
    std::vector<const char*> requiredExtensions_;
    bool shouldRequireDedicatedTransferQueue_ = false;
    bool shouldRequireDedicatedComputeQueue_ = false;
};
}
