#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

namespace yuubi {
class Viewport {
public:
    Viewport() {};
    Viewport(vk::SurfaceKHR surface, vk::PhysicalDevice physicalDevice, vk::Device device);
    ~Viewport();

    Viewport& operator=(Viewport&& rhs) = default;
    void recreateSwapChain();
    inline vk::RenderPass getRenderPass() { return renderPass_; }

private:
    struct SwapChainSupportDetails {
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> presentModes;
    };

    // --
    void createSwapChain();
    void destroySwapChain();
    SwapChainSupportDetails querySwapChainSupport(
        vk::PhysicalDevice physicalDevice) const;
    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(
        const std::vector<vk::SurfaceFormatKHR>& availableFormats) const;
    vk::PresentModeKHR chooseSwapPresentMode(
        const std::vector<vk::PresentModeKHR>& availablePresentModes) const;
    vk::Extent2D chooseSwapExtent(
        const vk::SurfaceCapabilitiesKHR& capabilities) const;
    void createImageViews();

    // --
    void createDepthStencil();
    vk::Format findDepthFormat();
    vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);
    void createImage(uint32_t width, uint32_t height, vk::Format format,
                     vk::ImageTiling tiling, vk::ImageUsageFlags usage,
                     vk::MemoryPropertyFlags properties, vk::Image& image,
                     vk::DeviceMemory& imageMemory);

    vk::ImageView createImageView(vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags);
    uint32_t findMemoryType(uint32_t typeFilter,
                            vk::MemoryPropertyFlags properties);

    // --
    void createFramebuffers();

    // -- 
    void createRenderPass();
    
    // Context
    vk::SurfaceKHR surface_;
    vk::PhysicalDevice physicalDevice_;
    vk::Device device_;

    // Swap chain resources
    vk::SwapchainKHR swapChain_;
    std::vector<vk::Image> swapChainImages_;
    std::vector<vk::ImageView> swapChainImageViews_;
    vk::Format swapChainImageFormat_;
    vk::Extent2D swapChainExtent_;

    // Depth buffer resources
    vk::Image depthImage_;
    vk::DeviceMemory depthImageMemory_;
    vk::ImageView depthImageView_;
    
    // Stencil buffer resources

    // Framebuffer resources
    std::vector<vk::Framebuffer> swapChainFramebuffers_;

    // Render pass
    vk::RenderPass renderPass_;
};
}
