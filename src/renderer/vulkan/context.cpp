#include "renderer/vulkan/context.h"
#include "renderer/vulkan/device.h"

#include "window.h"

#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace yuubi {

Context::Context(const Window& window) : window_(window) {
    createInstance();
    initDebugMessenger();
    createSurface();
}

Context::~Context() {
    instance_.destroySurfaceKHR(surface_);
    if (enableValidationLayers_) {
        instance_.destroyDebugUtilsMessengerEXT(debugMessenger_);
    }
    instance_.destroy();
}

void Context::createInstance() {
    if (enableValidationLayers_ && !checkValidationLayerSupport()) {
        UB_ERROR("Validation layers requested, but unavailable!");
    }
    vk::ApplicationInfo appInfo{
        .pApplicationName = "Yuubi",
        .applicationVersion = vk::makeApiVersion(0, 1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = vk::makeApiVersion(0, 1, 0, 0),
        .apiVersion = vk::ApiVersion10};

    auto extensions = getRequiredExtensions();

    vk::InstanceCreateInfo createInfo{
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = 0,
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };

    if (enableValidationLayers_) {
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

    instance_ = vk::createInstance(createInfo);
    dispatchLoaderDynamicInstance_.init(instance_, vkGetInstanceProcAddr);
}

bool Context::checkValidationLayerSupport() const {
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

std::vector<const char*> Context::getRequiredExtensions() const {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions,
                                        glfwExtensions + glfwExtensionCount);

    if (enableValidationLayers_) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

void Context::initDebugMessenger() {
    if (!enableValidationLayers_) {
        return;
    }

    vk::DebugUtilsMessengerCreateInfoEXT createInfo{
        .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                           vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                           vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning,
        .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                       vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                       vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
        .pfnUserCallback = debugCallback,
        .pUserData = nullptr};

    debugMessenger_ =
        instance_.createDebugUtilsMessengerEXT(createInfo, nullptr, dispatchLoaderDynamicInstance_);
}

void Context::createSurface() {
    VkSurfaceKHR tmp;

    if (glfwCreateWindowSurface(static_cast<VkInstance>(instance_),
                                window_.getWindow(), nullptr,
                                &tmp) != VK_SUCCESS) {
        UB_ERROR("Unable to create window surface!");
    }

    surface_ = vk::SurfaceKHR(tmp);
}

Device Context::createDevice() {
    return Device(this);
}

vk::Instance Context::getInstance() const {
    return instance_;
}

bool Context::deviceSupportsSurface(vk::PhysicalDevice physicalDevice, uint32_t index) const {
    return physicalDevice.getSurfaceSupportKHR(index, surface_);
}

SwapChainSupportDetails Context::querySwapChainSupport(
        vk::PhysicalDevice physicalDevice)
{
    return {.capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface_),
            .formats = physicalDevice.getSurfaceFormatsKHR(surface_),
            .presentModes = physicalDevice.getSurfacePresentModesKHR(surface_)};
}

std::vector<vk::PhysicalDevice> Context::enumeratePhysicalDevices() const
{
    return instance_.enumeratePhysicalDevices();
}

const std::vector<const char*> Context::validationLayers_ = {
    "VK_LAYER_KHRONOS_validation"};

}
