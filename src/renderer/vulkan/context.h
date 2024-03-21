#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

#include "window.h"

namespace yuubi {

class Device;

struct SwapChainSupportDetails {
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> presentModes;
};

class Context {
public:
    Context(const Window& window);
    ~Context();

    Device createDevice();
    vk::Instance getInstance() const;
    bool deviceSupportsSurface(vk::PhysicalDevice physicalDevice, uint32_t index) const;
    SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice physicalDevice);
    std::vector<vk::PhysicalDevice> enumeratePhysicalDevices() const;
private:
    void createInstance();
    void createSurface();
    void initDebugMessenger();
    bool checkValidationLayerSupport() const;
    std::vector<const char*> getRequiredExtensions() const;

    static VKAPI_ATTR VkBool32 VKAPI_CALL
    debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                  VkDebugUtilsMessageTypeFlagsEXT messageType,
                  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                  void* pUserData) {
        std::cerr << "validation layer: " << pCallbackData->pMessage
                  << std::endl;

        return VK_FALSE;
    }

    vk::DebugUtilsMessengerEXT debugMessenger_;
    vk::DispatchLoaderDynamic dispatchLoaderDynamicInstance_;
    static const std::vector<const char*> validationLayers_;
#if UB_DEBUG
    const bool enableValidationLayers_ = true;
#else
    const bool enableValidationLayers_ = false;
#endif
    vk::Instance instance_;
    vk::SurfaceKHR surface_;
    const Window& window_;
};

}
