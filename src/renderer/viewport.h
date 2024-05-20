#pragma once

#include "core/util.h"
#include "renderer/vulkan_usage.h"
#include "pch.h"
#include "renderer/vma/image.h"

namespace yuubi {

class Device;

struct Frame : NonCopyable {
    vk::raii::Semaphore imageAvailable = nullptr;
    vk::raii::Semaphore renderFinished = nullptr;
    vk::raii::Fence inFlight = nullptr;

    vk::raii::CommandPool commandPool = nullptr;
    vk::raii::CommandBuffer commandBuffer = nullptr;
};

struct SwapchainImage : NonCopyable {
    // Managed by swapchain
    vk::Image image = nullptr;
    vk::raii::ImageView imageView = nullptr;
};

class Viewport : NonCopyable {
public:
    Viewport(){};
    Viewport(std::shared_ptr<vk::raii::SurfaceKHR> surface,
             std::shared_ptr<Device> device);

    Viewport(Viewport&&) = default;
    Viewport& operator=(Viewport&& rhs);
    ~Viewport() = default;
    void recreateSwapChain();
    bool doFrame(std::function<void(const Frame&, const SwapchainImage&)> f);
    inline const vk::raii::ImageView& getDepthImageView() const { return depthImageView_; }
    inline const vk::Extent2D& getExtent() const { return swapChainExtent_; }

private:
    void createSwapChain();
    void createImageViews();
    void createDepthStencil();
    void createFrames();
    vk::SurfaceFormatKHR chooseSwapSurfaceFormat() const;
    vk::PresentModeKHR chooseSwapPresentMode() const;
    vk::Extent2D chooseSwapExtent() const;
    vk::Format findDepthFormat() const;
    vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates,
                                   vk::ImageTiling tiling,
                                   vk::FormatFeatureFlags features) const;
    inline const Frame& currentFrame() const { return frames_[currentFrame_]; }

    std::shared_ptr<vk::raii::SurfaceKHR> surface_;
    std::shared_ptr<Device> device_;
    vk::raii::SwapchainKHR swapChain_ = nullptr;
    std::vector<SwapchainImage> images_;
    vk::Format swapChainImageFormat_;
    vk::Extent2D swapChainExtent_;
    Image depthImage_;
    vk::raii::ImageView depthImageView_ = nullptr;
    
    static const uint32_t maxFramesInFlight_ = 2;
    std::array<Frame, maxFramesInFlight_> frames_;
    uint32_t currentFrame_ = 0;
    bool frameBufferResized_ = false;
};

}
