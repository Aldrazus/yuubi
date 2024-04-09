#include "renderer/instance.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace yuubi {
InstanceBuilder& InstanceBuilder::setAppName(std::string_view name)
{
    appName_ = name;
    return *this;
}
InstanceBuilder& InstanceBuilder::requireApiVersion(uint32_t apiVersion)
{
    apiVersion_ = apiVersion;
    return *this;
}
InstanceBuilder& InstanceBuilder::enableValidationLayers(bool shouldEnable)
{
    shouldEnableValidationLayers_ = shouldEnable;
    return *this;
}
InstanceBuilder& InstanceBuilder::useDefaultDebugMessenger()
{
    shouldUseDefaultDebugMessenger_ = true;
    return *this;
}

bool InstanceBuilder::supportsValidationLayers() {
std::vector<vk::LayerProperties> availableLayers =
        vk::enumerateInstanceLayerProperties();

    // TODO: find cleaner approach
    return std::all_of(
        validationLayers_.begin(), validationLayers_.end(),
        [&availableLayers](const std::string& layer) {
            return std::find_if(
                       availableLayers.begin(), availableLayers.end(),
                       [&layer](const vk::LayerProperties& availableLayer) {
                           return std::strcmp(availableLayer.layerName.data(),
                                              layer.data());
                       }) != availableLayers.end();
        });
}

InstanceBuilder& InstanceBuilder::requireExtensions(std::initializer_list<const char*> extensions) {
    requiredExtensions_ = extensions;
    return *this;
}

Instance InstanceBuilder::build()
{
    vk::ApplicationInfo appInfo{
        .pApplicationName = appName_.data(),
        .applicationVersion = vk::makeApiVersion(0, 1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = vk::makeApiVersion(0, 1, 0, 0),
        .apiVersion = apiVersion_};

    // extensions
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    for (uint32_t i = 0; i < glfwExtensionCount; i++) {
        requiredExtensions_.push_back(glfwExtensions[i]);
    }

    if (shouldEnableValidationLayers_) {
        requiredExtensions_.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    vk::InstanceCreateInfo createInfo{
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = 0,
        .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions_.size()),
        .ppEnabledExtensionNames = requiredExtensions_.data(),
    };

    if (shouldEnableValidationLayers_) {
        createInfo.enabledLayerCount = validationLayers_.size();
        createInfo.ppEnabledLayerNames = validationLayers_.data();

        vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo{
            .messageSeverity =
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning,
            .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                           vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                           vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
            .pfnUserCallback = debugCallback,
            .pUserData = nullptr};
        createInfo.pNext = &debugCreateInfo;
    }

    Instance instance{vk::createInstance(createInfo)};

    if (shouldEnableValidationLayers_ && shouldUseDefaultDebugMessenger_) {
        
        vk::DebugUtilsMessengerCreateInfoEXT createInfo{
            .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning,
            .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
            .pfnUserCallback = debugCallback,
            .pUserData = nullptr};

        vk::DispatchLoaderDynamic dldi;
        dldi.init(instance.instance, vkGetInstanceProcAddr);
        instance.debugMessenger =
            instance.instance.createDebugUtilsMessengerEXT(createInfo, nullptr, dldi);
    }

    return instance;
}

const std::vector<const char*> InstanceBuilder::validationLayers_ = {
    "VK_LAYER_KHRONOS_validation"};

}
