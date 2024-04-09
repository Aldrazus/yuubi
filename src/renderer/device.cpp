#include "renderer/device.h"
#include "renderer/device_selector.h"

namespace util {
uint32_t findGraphicsQueueFamilyIndex(vk::PhysicalDevice physicalDevice) {
    auto properties = physicalDevice.getQueueFamilyProperties();

    for (const auto [i, p] : std::views::enumerate(properties)) {
        if (p.queueFlags & vk::QueueFlagBits::eGraphics) {
            return i;
        }
    }
    // TODO: overflow
    UB_ERROR("Cannot find graphics queue family index");
    return -1;
}
}

namespace yuubi {
Device::Device(vk::Instance instance, const PhysicalDevice& physicalDevice)
    : instance_(instance), physicalDevice_(physicalDevice.physicalDevice) {
    float priority = 1.0f;
    const auto graphicsFamilyIndex =
        util::findGraphicsQueueFamilyIndex(physicalDevice.physicalDevice);
    vk::DeviceQueueCreateInfo queueCreateInfo{
        .queueFamilyIndex = graphicsFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &priority};

    vk::DeviceCreateInfo createInfo{
        .pNext = &physicalDevice.featuresToEnable,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledLayerCount = 0,
        .enabledExtensionCount =
            static_cast<uint32_t>(physicalDevice.extensionsToEnable.size()),
        .ppEnabledExtensionNames = physicalDevice.extensionsToEnable.data()};

    device_ = physicalDevice.physicalDevice.createDevice(createInfo);

    VmaAllocatorCreateInfo allocatorInfo{
        .physicalDevice = physicalDevice_,
        .device = device_,
        .instance = instance_,
        .vulkanApiVersion = vk::ApiVersion13,
    };
    vmaCreateAllocator(&allocatorInfo, &allocator_);
    graphicsQueue_ = {.queue = device_.getQueue(graphicsFamilyIndex, 0),
                      .familyIndex = graphicsFamilyIndex};
}

void Device::destroy() {
    vmaDestroyAllocator(allocator_);
    device_.destroy();
}

Buffer Device::createBuffer(size_t size, vk::BufferUsageFlags usage,
                            VmaMemoryUsage memoryUsage) {
    vk::BufferCreateInfo bufferInfo{.size = size, .usage = usage};

    VmaAllocationCreateInfo allocInfo{.usage = memoryUsage};

    Buffer buffer;
    vmaCreateBuffer(allocator_, &bufferInfo, &allocInfo, &buffer.buffer,
                    &buffer.allocation, nullptr);
}

}
