#pragma once

#include "core/util.h"
#include "renderer/vma/allocator.h"
#include "renderer/vulkan_usage.h"

namespace yuubi {
struct Queue {
    vk::raii::Queue queue = nullptr;
    uint32_t familyIndex;
};

struct Buffer {
    vk::raii::Buffer buffer;
    vma::Allocation allocation;
};

class Device : NonCopyable {
public:
    Device() = default;
    ~Device() = default;
    Device(Device&&) = default;
    Device& operator=(Device&&) = default;

    Device(const vk::raii::Instance& instance, const vk::raii::SurfaceKHR& surface);
private:
    void selectPhysicalDevice(const vk::raii::Instance& instance, const vk::raii::SurfaceKHR& surface);
    bool isDeviceSuitable(const vk::raii::PhysicalDevice& physicalDevice, const vk::raii::SurfaceKHR& surface);
    bool supportsFeatures(const vk::raii::PhysicalDevice& physicalDevice);
    void createLogicalDevice(const vk::raii::Instance& instance);

    vk::raii::PhysicalDevice physicalDevice_ = nullptr;
    vk::raii::Device device_ = nullptr;
    Queue graphicsQueue_;
    Allocator allocator_ = nullptr;
    
    static const vk::StructureChain<
        vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
        vk::PhysicalDeviceVulkan12Features, vk::PhysicalDeviceVulkan13Features>
        requiredFeatures_;

    static const std::vector<const char*> requiredExtensions_;
};
}
