#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

struct QueueFamilyIndices {
    std::optional<uint32_t> graphics_family;

    inline bool IsComplete() {
        return graphics_family.has_value();
    }
};

class Instance {
public:
    Instance();

    ~Instance();

    void pickPhysicalDevice();

    void createLogicalDevice();

private:
    static bool isDeviceSuitable(vk::PhysicalDevice device);

    static QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device);

    vk::Instance instance_;
    vk::PhysicalDevice physicalDevice_;
    vk::Device logicalDevice_;
    vk::Queue graphicsQueue_;
};
