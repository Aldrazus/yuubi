#pragma once

#include <vulkan/vulkan.hpp>

#include "pch.h"
#include "window.h"

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;

    inline bool isComplete() {
        return graphicsFamily.has_value();
    }
};
class Renderer {
public:
    Renderer(const Window& window);

    ~Renderer();

private:
    void createInstance();

    void createSurface();

    void pickPhysicalDevice();

    void createLogicalDevice();

    QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice physicalDevice);

    bool isDeviceSuitable(vk::PhysicalDevice device);

    void setupDebugMessenger();

    bool checkValidationLayerSupport();

    std::vector<const char*> getRequiredExtensions();

    static VKAPI_ATTR VkBool32 VKAPI_CALL
    debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                  VkDebugUtilsMessageTypeFlagsEXT messageType,
                  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                  void* pUserData) {
        std::cerr << "validation layer: " << pCallbackData->pMessage
                  << std::endl;

        return VK_FALSE;
    }

    vk::Instance instance_;
    vk::DebugUtilsMessengerEXT debugMessenger_;
    vk::DispatchLoaderDynamic dldi_;
    static std::vector<const char*> validationLayers_;
#if UB_DEBUG
    const bool enableValidationLayers_ = true;
#else
    const bool enableValidationLayers_ = false;
#endif

    vk::PhysicalDevice physicalDevice_;
    vk::Device device_;
    vk::Queue graphicsQueue_;
    const Window& window_;
    vk::SurfaceKHR surface_;
};
