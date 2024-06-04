#pragma once

#include "core/util.h"
#include "renderer/vma/allocator.h"
#include "renderer/vulkan_usage.h"
#include "renderer/vma/buffer.h"

namespace yuubi {
struct Queue {
    vk::raii::Queue queue = nullptr;
    uint32_t familyIndex;
};

class Image;
class Device : NonCopyable {
public:
    Device() = default;
    ~Device() = default;
    Device(Device&&) = default;
    Device& operator=(Device&&) = default;
    Device(const vk::raii::Instance& instance, const vk::raii::SurfaceKHR& surface);

    const vk::raii::Device& getDevice() const { return device_; }
    const vk::raii::PhysicalDevice& getPhysicalDevice() const { return physicalDevice_; }

    Image createImage(uint32_t width, uint32_t height, vk::Format format,
                      vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties);

    vk::raii::ImageView createImageView(const vk::Image& image, const vk::Format& format, vk::ImageAspectFlags aspectFlags);

    Buffer createBuffer(const vk::BufferCreateInfo& createInfo, const vma::AllocationCreateInfo& allocInfo);

    const Queue& getQueue() {
        return graphicsQueue_;
    }

private:
    void selectPhysicalDevice(const vk::raii::Instance& instance, const vk::raii::SurfaceKHR& surface);
    bool isDeviceSuitable(const vk::raii::PhysicalDevice& physicalDevice, const vk::raii::SurfaceKHR& surface);
    bool supportsFeatures(const vk::raii::PhysicalDevice& physicalDevice);
    void createLogicalDevice(const vk::raii::Instance& instance);

    vk::raii::PhysicalDevice physicalDevice_ = nullptr;
    vk::raii::Device device_ = nullptr;
    Queue graphicsQueue_;
    std::shared_ptr<Allocator> allocator_ = nullptr;
    
    static const vk::StructureChain<
        vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
        vk::PhysicalDeviceVulkan12Features, vk::PhysicalDeviceVulkan13Features>
        requiredFeatures_;

    static const std::vector<const char*> requiredExtensions_;
};
}
