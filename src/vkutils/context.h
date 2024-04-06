#pragma once

#include "vulkan_usage.h"
#include "window.h"
#include "pch.h"

namespace yuubi {
namespace vkutils {

struct SwapChainSupportDetails {
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> presentModes;
};

class Device;

// Encapsulates all resources necessary for rendering with Vulkan.
// Can be abstracted as API-agnostic interface in the future.
class Context {
public:
    Context(const Window& window);

    inline vk::Instance getInstance() const { return instance_; }
    inline vk::SurfaceKHR getSurface() const { return surface_; }
    inline Device* getDevice() const { return device_; }
    inline GLFWwindow* getWindow() const { return window_.getWindow(); }
    inline std::vector<vk::PhysicalDevice> getPhysicalDevices() const { return instance_.enumeratePhysicalDevices(); }
    inline bool physicalDeviceSupportsSurface(const vk::PhysicalDevice& physicalDevice, uint32_t queueFamilyIndex) const { return physicalDevice.getSurfaceSupportKHR(queueFamilyIndex, surface_); }
    void destroy();

private:
    void createInstance();
    void createSurface();
    bool checkValidationLayerSupport();
    std::vector<const char*> getRequiredExtensions();

    void setupDebugMessenger();
    static VKAPI_ATTR VkBool32 VKAPI_CALL
    debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                  VkDebugUtilsMessageTypeFlagsEXT messageType,
                  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                  void* pUserData) {
        std::cerr << "validation layer: " << pCallbackData->pMessage
                  << std::endl;

        return VK_FALSE;
    }


    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        inline bool isComplete() {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    vk::PhysicalDevice selectPhysicalDevice();
    void createDevice();
    QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice physicalDevice);
    bool isDeviceSuitable(vk::PhysicalDevice device);
    bool checkDeviceExtensionSupport(vk::PhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupport(
        vk::PhysicalDevice physicalDevice);

    static const std::vector<const char*> deviceExtensions_;

    vk::Instance instance_;
    vk::DebugUtilsMessengerEXT debugMessenger_;
    const Window& window_;
    vk::SurfaceKHR surface_;
    vk::DispatchLoaderDynamic dldi_;

    QueueFamilyIndices queueFamilyIndices_;
    // TODO: use smart pointers
    Device* device_;

    static const std::vector<const char*> validationLayers_;
#if UB_DEBUG
    const bool enableValidationLayers_ = true;
#else
    const bool enableValidationLayers_ = false;
#endif
};

} // namespace yuubi
} // namespace vkutils
