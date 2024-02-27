#pragma once

#include <vulkan/vulkan.hpp>

#include "pch.h"
#include "window.h"

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    inline bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> presentModes;
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

    bool checkDeviceExtensionSupport(vk::PhysicalDevice device);

    SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice physicalDevice);

    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);

    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);

    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);

    void createSwapChain();

    void createImageViews();

    void createRenderPass();

    void createGraphicsPipeline();

    static std::vector<char> readFile(const std::string& filename);

    vk::ShaderModule createShaderModule(const std::vector<char>& code);

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
    static const std::vector<const char*> validationLayers_;
#if UB_DEBUG
    const bool enableValidationLayers_ = true;
#else
    const bool enableValidationLayers_ = false;
#endif

    vk::PhysicalDevice physicalDevice_;
    vk::Device device_;
    vk::Queue graphicsQueue_;
    vk::Queue presentQueue_;
    const Window& window_;
    vk::SurfaceKHR surface_;

    static const std::vector<const char*> deviceExtensions_;

    vk::SwapchainKHR swapChain_;
    std::vector<vk::Image> swapChainImages_;
    vk::Format swapChainImageFormat_;
    vk::Extent2D swapChainExtent_;
    std::vector<vk::ImageView> swapChainImageViews_;
    vk::RenderPass renderPass_;
    vk::PipelineLayout pipelineLayout_;
    vk::Pipeline graphicsPipeline_;
};
