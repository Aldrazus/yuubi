#pragma once

#include "renderer/vulkan_usage.h"

#include <glm/glm.hpp>

#include "pch.h"
#include "renderer/viewport.h"
#include "window.h"
#include "renderer/device_selector.h"

class Device;
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    static vk::VertexInputBindingDescription getBindingDescription();
    static std::array<vk::VertexInputAttributeDescription, 3>
    getAttributeDescriptions();
};

const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {
    0, 1, 2, 2, 3, 0,
    4, 5, 6, 6, 7, 4
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

    inline void resize() { viewport_.shouldResize(); }

private:

    SwapChainSupportDetails querySwapChainSupport(
        vk::PhysicalDevice physicalDevice);

    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(
        const std::vector<vk::SurfaceFormatKHR>& availableFormats);

    vk::PresentModeKHR chooseSwapPresentMode(
        const std::vector<vk::PresentModeKHR>& availablePresentModes);

    vk::Extent2D chooseSwapExtent(
        const vk::SurfaceCapabilitiesKHR& capabilities);

    void createDescriptorSetLayout();

    void createUniformBuffers();

    void updateUniformBuffer(uint32_t currentImage);

    void createDescriptorPool();

    void createDescriptorSets();

    void createGraphicsPipeline();

    void createCommandPool();

    vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);

    bool hasStencilComponent(vk::Format format);

    void createTextureImage();

    void createTextureImageView();

    void createTextureSampler();

    vk::ImageView createImageView(vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags);

    vk::CommandBuffer beginSingleTimeCommands();

    void endSingleTimeCommands(vk::CommandBuffer commandBuffer);

    void transitionImageLayout(vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);

    void copyBufferToImage(vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height);

    void createImage(uint32_t width, uint32_t height, vk::Format format,
                     vk::ImageTiling tiling, vk::ImageUsageFlags usage,
                     vk::MemoryPropertyFlags properties, vk::Image& image,
                     vk::DeviceMemory& imageMemory);

    void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                      vk::MemoryPropertyFlags properties, vk::Buffer& buffer,
                      vk::DeviceMemory& bufferMemory);

    void copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer,
                    vk::DeviceSize size);

    void createVertexBuffer();

    void createIndexBuffer();

    uint32_t findMemoryType(uint32_t typeFilter,
                            vk::MemoryPropertyFlags properties);

    void createCommandBuffers();

    void recordCommandBuffer(vk::CommandBuffer commandBuffer);

    vk::ShaderModule createShaderModule(const std::vector<char>& code);

    const Window& window_;

    vk::Instance instance_;
    vk::DispatchLoaderDynamic dldi_;
    vk::DispatchLoaderDynamic dldy_;
    vk::DebugUtilsMessengerEXT debugMessenger_;
    vk::SurfaceKHR surface_;

    vk::PhysicalDevice physicalDevice_;
    vk::Device device_;
    VmaAllocator allocator_;
    vk::Queue graphicsQueue_;
    uint32_t graphicsQueueFamilyIndex_;

    yuubi::Viewport viewport_;

    static const std::vector<const char*> deviceExtensions_;

    vk::DescriptorSetLayout descriptorSetLayout_;
    vk::PipelineLayout pipelineLayout_;
    vk::Pipeline graphicsPipeline_;

    vk::CommandPool commandPool_;
    std::vector<vk::CommandBuffer> commandBuffers_;

    vk::Buffer vertexBuffer_;
    vk::DeviceMemory vertexBufferMemory_;
    vk::Buffer indexBuffer_;
    vk::DeviceMemory indexBufferMemory_;

    std::vector<vk::Buffer> uniformBuffers_;
    std::vector<vk::DeviceMemory> uniformBuffersMemory_;
    std::vector<void*> uniformBuffersMapped_;

    vk::DescriptorPool descriptorPool_;
    std::vector<vk::DescriptorSet> descriptorSets_;

    vk::Image textureImage_;
    vk::DeviceMemory textureImageMemory_;

    vk::ImageView textureImageView_;
    vk::Sampler textureSampler_;
};
