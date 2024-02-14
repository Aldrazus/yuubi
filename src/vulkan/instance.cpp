// For designated initializers
#include "log.h"
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

#include "instance.h"
#include "pch.h"

Instance::Instance() {
    vk::ApplicationInfo appInfo{.pApplicationName = "Yuubi",
                                .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                                .pEngineName = "No Engine",
                                .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                                .apiVersion = VK_API_VERSION_1_0};

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    vk::InstanceCreateInfo instanceCreateInfo{
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = 0,
        .enabledExtensionCount = glfwExtensionCount,
        .ppEnabledExtensionNames = glfwExtensions,
    };

    // TODO: handle error
    instance_ = vk::createInstance(instanceCreateInfo);
}

Instance::~Instance() {
    logicalDevice_.destroy();
    instance_.destroy();
};

void Instance::pickPhysicalDevice() {
    std::vector<vk::PhysicalDevice> devices =
        instance_.enumeratePhysicalDevices();
    auto physical_device_iter =
        std::ranges::find_if(devices, Instance::isDeviceSuitable);

    if (physical_device_iter == devices.end()) {
        // TODO: handle error
        UB_ERROR("Failed to find a suitable GPU!");
    }
    physicalDevice_ = *physical_device_iter;
}

bool Instance::isDeviceSuitable(vk::PhysicalDevice device) {
    QueueFamilyIndices indices = findQueueFamilies(device);

    return indices.IsComplete();
}

QueueFamilyIndices Instance::findQueueFamilies(vk::PhysicalDevice device) {
    auto properties = device.getQueueFamilyProperties();
    auto graphics_queue_family_iter =
        std::find_if(properties.begin(), properties.end(),
                     [](const vk::QueueFamilyProperties& p) {
                         return p.queueFlags & vk::QueueFlagBits::eGraphics;
                     });

    if (graphics_queue_family_iter != properties.end()) {
        return { .graphics_family = std::distance(properties.begin(), graphics_queue_family_iter) };
    }

    return {};
}

void Instance::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);
    float queuePriority = 1.0f;

    vk::DeviceQueueCreateInfo queueCreateInfo{
        .queueFamilyIndex = indices.graphics_family.value(),
        .queueCount = 1,
        .pQueuePriorities = &queuePriority
    };

    vk::DeviceCreateInfo createInfo{
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledLayerCount = 0,
        .enabledExtensionCount = 0,
        .pEnabledFeatures = {},
    };

    logicalDevice_ = physicalDevice_.createDevice(createInfo);
    graphicsQueue_ = logicalDevice_.getQueue(indices.graphics_family.value(), 0);
}
