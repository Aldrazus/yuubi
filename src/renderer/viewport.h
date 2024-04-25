#pragma once

#include "core/util.h"
#include "renderer/vulkan_usage.h"
#include "pch.h"

namespace yuubi {

class Device;

class Viewport : NonCopyable {
public:
    Viewport() {};
    Viewport(std::shared_ptr<vk::raii::SurfaceKHR> surface, std::shared_ptr<Device> device);

    void recreateSwapChain();

private:
    struct FrameData {
        // TODO: add command pool and command buffer
        vk::raii::Semaphore imageAvailable_;
        vk::raii::Semaphore renderFinished_;
        vk::raii::Fence inFlight_;
    };
    
    void createSwapChain();
    void createImageViews();
    void createDepthStencil();
    vk::SurfaceFormatKHR chooseSwapSurfaceFormat() const;
    vk::PresentModeKHR chooseSwapPresentMode() const;
    vk::Extent2D chooseSwapExtent() const;

    std::shared_ptr<vk::raii::SurfaceKHR> surface_;
    std::shared_ptr<Device> device_;
    vk::raii::SwapchainKHR swapChain_ = nullptr;
    std::vector<vk::raii::ImageView> imageViews_;
    vk::Format swapChainImageFormat_;
    vk::Extent2D swapChainExtent_;

    bool doFrame();
};

}

#if 0

#include <cstddef>
#include "core/util.h"
#include "renderer/vulkan_usage.h"

#define MAX_FRAMES_IN_FLIGHT 2

namespace yuubi {

class Viewport : NonCopyable {
public:
    Viewport(std::nullptr_t) {};
    Viewport(vk::SurfaceKHR surface, vk::PhysicalDevice physicalDevice, vk::Device device, vk::Queue presentQueue);

    void initialize();
    void destroy();

    void recreateSwapChain();

    inline vk::Extent2D getExtent() const {
        return swapChainExtent_;
    }

    inline vk::ImageView getDepthImageView() const {
        return depthImageView_;
    }

    inline vk::Format getDepthFormat() const {
        return depthFormat_;
    }

    inline vk::Format getSwapChainImageFormat() const {
        return swapChainImageFormat_;
    }

    inline vk::Image getCurrentImage() const {
        return swapChainImages_[imageIndex_];
    }

    inline vk::ImageView getCurrentImageView() const {
        return swapChainImageViews_[imageIndex_];
    }

    inline vk::Fence getCurrentFrameFence() const {
        return inFlightFences_[currentFrame_];
    }

    inline vk::Semaphore getImageAvailableSemaphore() const {
        return imageAvailableSemaphores_[currentFrame_];
    }

    inline vk::Semaphore getRenderFinishedSemaphore() const {
        return renderFinishedSemaphores_[currentFrame_];
    }

    inline uint32_t getCurrentFrame() const {
        return currentFrame_;
    }

    inline void shouldResize() {
        framebufferResized_ = true;
    }

    // TODO: find more elegant solution
    bool beginFrame();
    bool endFrame();

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
    void createSyncObjects();

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

    // Context
    const vk::raii::SurfaceKHR& surface_ = nullptr;
    const vk::raii::PhysicalDevice& physicalDevice_ = nullptr;
    const vk::raii::Device& device_ = nullptr;
    const vk::raii::Queue& presentQueue_ = nullptr;
    const vk::raii::SwapchainKHR swapChain_ = nullptr;


    vk::SurfaceKHR surface_;
    vk::PhysicalDevice physicalDevice_;
    vk::Device device_;
    vk::Queue presentQueue_;

    // Swap chain resources
    vk::SwapchainKHR swapChain_;
    std::vector<vk::Image> swapChainImages_;
    std::vector<vk::ImageView> swapChainImageViews_;
    vk::Format swapChainImageFormat_;
    vk::Extent2D swapChainExtent_;

    // Current frame being rendered
    uint32_t currentFrame_ = 0;

    // Index of current image being rendered to
    uint32_t imageIndex_;

    bool framebufferResized_ = false;

    // Depth buffer resources
    vk::Image depthImage_;
    vk::DeviceMemory depthImageMemory_;
    vk::ImageView depthImageView_;
    vk::Format depthFormat_;
    
    // Stencil buffer resources

    // Sync objects
    std::vector<vk::Semaphore> imageAvailableSemaphores_;
    std::vector<vk::Semaphore> renderFinishedSemaphores_;
    std::vector<vk::Fence> inFlightFences_;
};

} // namespace yuubi
#endif
