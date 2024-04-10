#pragma once

#include <initializer_list>
#include "renderer/vulkan_usage.h"
#include "pch.h"

namespace yuubi {
struct Instance {
    vk::Instance instance;
    vk::DebugUtilsMessengerEXT debugMessenger;
};

class InstanceBuilder {
public:
    InstanceBuilder& setAppName(std::string_view name);
    InstanceBuilder& requireApiVersion(uint32_t apiVersion);
    InstanceBuilder& enableValidationLayers(bool shouldEnable);
    InstanceBuilder& requireExtensions(std::initializer_list<const char*> extensions);
    InstanceBuilder& useDefaultDebugMessenger();
    Instance build();
private:
    bool supportsValidationLayers();
    static VKAPI_ATTR VkBool32 VKAPI_CALL
    debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                  VkDebugUtilsMessageTypeFlagsEXT messageType,
                  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                  void* pUserData) {
        std::cerr << "validation layer: " << pCallbackData->pMessage
                  << std::endl;

        return VK_FALSE;
    }
    std::string appName_ = "Untitled App";
    uint32_t apiVersion_ = vk::ApiVersion10;
    bool shouldEnableValidationLayers_ = false;
    bool shouldUseDefaultDebugMessenger_ = false;
    std::vector<const char*> requiredExtensions_;
    static const std::vector<const char*> validationLayers_;
};
}
