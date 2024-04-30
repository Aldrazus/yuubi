#pragma once

#include "core/util.h"
#include "renderer/vulkan_usage.h"
#include "pch.h"
#include "renderer/vma/image.h"

namespace yuubi {

class Device;

struct Frame : NonCopyable {
    vk::raii::Semaphore imageAvailable_ = nullptr;
    vk::raii::Semaphore renderFinished_ = nullptr;
    vk::raii::Fence inFlight_ = nullptr;

    vk::raii::CommandPool commandPool_ = nullptr;
    vk::raii::CommandBuffer commandBuffer_ = nullptr;
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

    bool doFrame(std::function<void(const Frame&, const vk::raii::ImageView&)> f);

    std::shared_ptr<vk::raii::SurfaceKHR> surface_;
    std::shared_ptr<Device> device_;
    vk::raii::SwapchainKHR swapChain_ = nullptr;
    std::vector<vk::raii::ImageView> imageViews_;
    vk::Format swapChainImageFormat_;
    vk::Extent2D swapChainExtent_;
    Image depthImage_;
    
    static const uint32_t maxFramesInFlight_ = 2;
    std::array<Frame, maxFramesInFlight_> frames_;
    uint32_t currentFrame_ = 0;
    bool frameBufferResized_ = false;
};

}
