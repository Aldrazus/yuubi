#pragma once

#include "core/util.h"
#include "renderer/vma/allocator.h"
#include "renderer/vulkan_usage.h"
#include "renderer/vma/buffer.h"

namespace yuubi {

class Buffer;
class Image;
struct ImageCreateInfo;

struct Queue {
    vk::raii::Queue queue = nullptr;
    uint32_t familyIndex;
};

class Device : NonCopyable {
public:
    Device() = default;
    ~Device() = default;
    Device(Device&&) = default;
    Device& operator=(Device&&) = default;
    Device(const vk::raii::Instance& instance, const vk::raii::SurfaceKHR& surface);

    const vk::raii::Device& getDevice() const { return device_; }
    const vk::raii::PhysicalDevice& getPhysicalDevice() const { return physicalDevice_; }

    vk::raii::ImageView createImageView(const vk::Image& image, const vk::Format& format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels = 1, vk::ImageViewType type = vk::ImageViewType::e2D
    ) const;

    const Queue& getQueue() {
        return graphicsQueue_;
    }

    inline Allocator& allocator() const { return *allocator_; } 

    Image createImage(const ImageCreateInfo& createInfo) const;
    Buffer createBuffer(const vk::BufferCreateInfo& createInfo, const VmaAllocationCreateInfo& allocInfo) const;
    void submitImmediateCommands(std::function<void(const vk::raii::CommandBuffer& commandBuffer)>&& function) const;

private:
    void selectPhysicalDevice(const vk::raii::Instance& instance, const vk::raii::SurfaceKHR& surface);
    bool isDeviceSuitable(const vk::raii::PhysicalDevice& physicalDevice, const vk::raii::SurfaceKHR& surface);
    static bool supportsFeatures(const vk::raii::PhysicalDevice& physicalDevice);
    void createLogicalDevice(const vk::raii::Instance& instance);
    void createImmediateCommandResources();

    vk::raii::PhysicalDevice physicalDevice_ = nullptr;
    vk::raii::Device device_ = nullptr;
    Queue graphicsQueue_;
    std::shared_ptr<Allocator> allocator_ = nullptr;

    // Immediate Commands
    vk::raii::CommandPool immediateCommandPool_ = nullptr;
    vk::raii::CommandBuffer immediateCommandBuffer_ = nullptr;
    vk::raii::Fence immediateCommandFence_ = nullptr;
    
    static const vk::StructureChain<
        vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
        vk::PhysicalDeviceVulkan12Features, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceDynamicRenderingLocalReadFeaturesKHR>
        requiredFeatures_;

    static const std::vector<const char*> requiredExtensions_;
};
}
