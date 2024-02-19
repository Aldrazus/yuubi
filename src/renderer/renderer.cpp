#include <cstring>
#include "GLFW/glfw3.h"

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

#include "renderer/renderer.h"
#include "pch.h"

Renderer::Renderer() {
    createInstance();
    setupDebugMessenger();
    pickPhysicalDevice();
}

Renderer::~Renderer() {
    if (enableValidationLayers_) {
        instance_.destroyDebugUtilsMessengerEXT(debugMessenger_, nullptr,
                                                dldi_);
    }
    instance_.destroy();
}

void Renderer::createInstance() {
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
}

void Renderer::pickPhysicalDevice() {
    std::vector<vk::PhysicalDevice> physicalDevices =
        instance_.enumeratePhysicalDevices();

    auto suitableDeviceIter = std::find_if(
        physicalDevices.begin(), physicalDevices.end(),
        [this](vk::PhysicalDevice device) { return isDeviceSuitable(device); });

    if (suitableDeviceIter == physicalDevices.end()) {
        UB_ERROR("Failed to find a suitable GPU!");
    }

    physicalDevice_ = *suitableDeviceIter;
}

QueueFamilyIndices Renderer::findQueueFamilies(vk::PhysicalDevice physicalDevice) {
    QueueFamilyIndices indices;

    std::vector<vk::QueueFamilyProperties> properties = physicalDevice.getQueueFamilyProperties();

    auto graphicsQueueIter = std::find_if(properties.begin(), properties.end(), [](vk::QueueFamilyProperties p){
        return p.queueFlags & vk::QueueFlagBits::eGraphics;
    });

    if (graphicsQueueIter != properties.end()) {
        indices.graphicsFamily = std::distance(properties.begin(), graphicsQueueIter);
    }

    return indices;
}

bool Renderer::isDeviceSuitable(vk::PhysicalDevice physicalDevice) {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    return indices.isComplete();
}

bool Renderer::checkValidationLayerSupport() {
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

std::vector<const char*> Renderer::getRequiredExtensions() {
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

void Renderer::setupDebugMessenger() {
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

    dldi_.init(instance_, vkGetInstanceProcAddr);
    debugMessenger_ =
        instance_.createDebugUtilsMessengerEXT(createInfo, nullptr, dldi_);
    // TODO:
    /*
    device = physicalDevice.createDevice(..., allocator, dldy);
    dldy.init(device);
    */
}

std::vector<const char*> Renderer::validationLayers_ = {
    "VK_LAYER_KHRONOS_validation"};
