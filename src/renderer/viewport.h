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
        Viewport() {};
        Viewport(std::shared_ptr<vk::raii::SurfaceKHR> surface, std::shared_ptr<Device> device);

        Viewport(Viewport&&) = default;
        Viewport& operator=(Viewport&& rhs);
        ~Viewport() = default;
        void recreateSwapChain();
        bool doFrame(
                std::function<void(const Frame&, const SwapchainImage&, const Image&, const vk::raii::ImageView&)> f
        );
        [[nodiscard]] const Image& getDepthImage() const { return depthImage_; }
        [[nodiscard]] const vk::raii::ImageView& getDepthImageView() const { return depthImageView_; }
        [[nodiscard]] const vk::Extent2D& getExtent() const { return swapChainExtent_; }
        [[nodiscard]] const vk::Format& getSwapChainImageFormat() const { return swapChainImageFormat_; }

        [[nodiscard]] const vk::raii::ImageView& getDrawImageView() const { return drawImageView_; }

        [[nodiscard]] const vk::Format& getDrawImageFormat() const { return drawImageFormat_; }

        [[nodiscard]] const Image& getNormalImage() const { return normalImage_; }
        [[nodiscard]] const vk::raii::ImageView& getNormalImageView() const { return normalImageView_; }
        [[nodiscard]] const vk::Format& getNormalImageFormat() const { return normalImageFormat_; }

        [[nodiscard]] const Image& getAOImage() const { return aoImage_; }
        [[nodiscard]] const vk::raii::ImageView& getAOImageView() const { return aoImageView_; }
        [[nodiscard]] const vk::Format& getAOImageFormat() const { return aoImageFormat_; }

        [[nodiscard]] inline const vk::Format& getDepthFormat() const { return depthImageFormat_; }
        static const uint32_t maxFramesInFlight = 2;

    private:
        void createSwapChain();
        void createImageViews();
        void createDepthStencil();
        void createDrawImage();
        void createNormalImage();
        void createAOImage();
        void createFrames();
        vk::SurfaceFormatKHR chooseSwapSurfaceFormat() const;
        vk::PresentModeKHR chooseSwapPresentMode() const;
        vk::Extent2D chooseSwapExtent() const;
        vk::Format findDepthFormat() const;
        vk::Format findSupportedFormat(
                const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features
        ) const;
        inline const Frame& currentFrame() const { return frames_[currentFrame_]; }

        std::shared_ptr<vk::raii::SurfaceKHR> surface_;
        std::shared_ptr<Device> device_;
        vk::raii::SwapchainKHR swapChain_ = nullptr;
        std::vector<SwapchainImage> images_;
        vk::Format swapChainImageFormat_;
        vk::Extent2D swapChainExtent_;
        Image depthImage_;
        vk::raii::ImageView depthImageView_ = nullptr;
        vk::Format depthImageFormat_;

        Image drawImage_;
        vk::raii::ImageView drawImageView_ = nullptr;
        vk::Format drawImageFormat_ = vk::Format::eR16G16B16A16Sfloat;

        Image normalImage_;
        vk::raii::ImageView normalImageView_ = nullptr;
        // TODO: change format to eR16G16Sfloat eventually
        // eR16G16B16Sfloat is not well supported with optimal image tiling,
        // so the alpha component is added.
        // See: https://vulkan.gpuinfo.org/listoptimaltilingformats.php
        vk::Format normalImageFormat_ = vk::Format::eR16G16B16A16Sfloat;

        Image aoImage_;
        vk::raii::ImageView aoImageView_ = nullptr;
        vk::Format aoImageFormat_ = vk::Format::eR16G16B16A16Sfloat;

        std::array<Frame, maxFramesInFlight> frames_;
        uint32_t currentFrame_ = 0;
        bool frameBufferResized_ = false;
    };

}
