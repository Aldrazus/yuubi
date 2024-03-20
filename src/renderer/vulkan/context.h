#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

namespace yuubi {

class Context {
public:
    Context();
    ~Context();
    void createSurface();
    void initDebugMessenger();

private:
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
};

}
