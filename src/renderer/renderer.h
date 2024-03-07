#pragma once

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

#include "pch.h"
#include "window.h"

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;

    static vk::VertexInputBindingDescription getBindingDescription();
    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions();
};

const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {
    0, 1, 2, 2, 3, 0
};

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

    void drawFrame();

    inline void resize() {
        framebufferResized_ = true;
    }
private:
    static const uint32_t maxFramesInFlight_ = 2;

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

    void createDescriptorSetLayout();

    void createUniformBuffers();
    
    void updateUniformBuffer(uint32_t currentImage);

    void createDescriptorPool();

    void createDescriptorSets();

    void createGraphicsPipeline();

    void createFramebuffers();

    void createCommandPool();

    void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Buffer& buffer, vk::DeviceMemory& bufferMemory);

    void copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size);

    void createVertexBuffer();

    void createIndexBuffer();

    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);

    void createCommandBuffers();

    void recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex);

    void createSyncObjects();

    void cleanupSwapChain();

    void recreateSwapChain();

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
    vk::DescriptorSetLayout descriptorSetLayout_;
    vk::PipelineLayout pipelineLayout_;
    vk::Pipeline graphicsPipeline_;

    std::vector<vk::Framebuffer> swapChainFramebuffers_;
    vk::CommandPool commandPool_;
    std::vector<vk::CommandBuffer> commandBuffers_;
    
    std::vector<vk::Semaphore> imageAvailableSemaphores_;
    std::vector<vk::Semaphore> renderFinishedSemaphores_;
    std::vector<vk::Fence> inFlightFences_;

    bool framebufferResized_ = false;

    uint32_t currentFrame_ = 0;

    vk::Buffer vertexBuffer_;
    vk::DeviceMemory vertexBufferMemory_;
    vk::Buffer indexBuffer_;
    vk::DeviceMemory indexBufferMemory_;

    std::vector<vk::Buffer> uniformBuffers_;
    std::vector<vk::DeviceMemory> uniformBuffersMemory_;
    std::vector<void*> uniformBuffersMapped_;

    vk::DescriptorPool descriptorPool_;
    std::vector<vk::DescriptorSet> descriptorSets_;
};
