#pragma once

#include "vkutils/vulkan_usage.h"
#include "vkutils/context.h"

namespace yuubi {
namespace vkutils {

struct Queue {
    vk::Queue queue;
    uint32_t familyIndex;
    inline operator vk::Queue() const { return queue; }
};

class Device {
public:
    Device(Context* context, vk::PhysicalDevice physicalDevice, const std::vector<const char*>& extensions);
    ~Device();
    inline vk::Device getDevice() const { return device_; }
    inline vk::PhysicalDevice getPhysicalDevice() const { return physicalDevice_; }
    inline vk::Queue getGraphicsQueue() const { return graphicsQueue_; }
    inline uint32_t getGraphicsQueueFamily() const { return graphicsQueue_.familyIndex; }
    inline vk::DispatchLoaderDynamic getDispatchLoader() const { return dldy_; }
private:
    void createLogicalDevice();

    Context* context_;
    vk::PhysicalDevice physicalDevice_;
    const std::vector<const char*> extensions_;
    vk::Device device_;
    // Used for graphics and presenting
    Queue graphicsQueue_;
    vk::DispatchLoaderDynamic dldy_;
};
} // namespace vkutils
} // namespace yuubi
