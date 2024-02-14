// For designated initializers
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

#include "instance.h"
#include "pch.h"

Instance::Instance() {
    vk::ApplicationInfo appInfo{
        .pApplicationName   = "Yuubi",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName        = "No Engine",
        .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion         = VK_API_VERSION_1_0
    };

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    vk::InstanceCreateInfo instanceCreateInfo{ 
        .pApplicationInfo       = &appInfo, 
        .enabledLayerCount      = 0,
        .enabledExtensionCount  = glfwExtensionCount,
        .ppEnabledExtensionNames    = glfwExtensions,
    };

    // TODO: handle error
    instance_ = vk::createInstanceUnique(instanceCreateInfo);
}

Instance::~Instance() = default;
