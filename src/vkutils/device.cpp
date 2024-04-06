#include "vkutils/device.h"
#include "vkutils/context.h"

namespace yuubi {
namespace vkutils {

Device::Device(Context* context, vk::PhysicalDevice physicalDevice, const std::vector<const char*>& extensions) : context_(context), physicalDevice_(physicalDevice), extensions_(extensions) {
    createLogicalDevice();
}

Device::~Device() {
    device_.destroy();
}

void Device::createLogicalDevice() {
    // Find graphics + present queue family
    auto properties = physicalDevice_.getQueueFamilyProperties();
    uint32_t graphicsFamilyIndex;

    for (const auto [i, p] : std::views::enumerate(properties)) {
        if (p.queueFlags & vk::QueueFlagBits::eGraphics) {
            graphicsFamilyIndex = i;
            break;
        }
    }

    float priority = 1.0f;
    vk::DeviceQueueCreateInfo queueCreateInfo{
        .queueFamilyIndex = graphicsFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &priority,
    };

    vk::PhysicalDeviceFeatures deviceFeatures{.samplerAnisotropy = vk::True};

    vk::PhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeature{
        .dynamicRendering = vk::True
    };

    vk::DeviceCreateInfo createInfo{
        .pNext = &dynamicRenderingFeature,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledLayerCount = 0,
        .enabledExtensionCount =
            static_cast<uint32_t>(extensions_.size()),
        .ppEnabledExtensionNames = extensions_.data(),
        .pEnabledFeatures = &deviceFeatures,
    };

    device_ = physicalDevice_.createDevice(createInfo);
    graphicsQueue_ = {
        .queue = device_.getQueue(graphicsFamilyIndex, 0),
        .familyIndex = graphicsFamilyIndex
    };
    dldy_.init(context_->getInstance(), vkGetInstanceProcAddr, device_, vkGetDeviceProcAddr);
}

} // namespace vkutils
} // namespace yuubi
