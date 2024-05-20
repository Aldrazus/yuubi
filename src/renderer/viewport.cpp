#include "renderer/viewport.h"

#include <GLFW/glfw3.h>
#include "renderer/vulkan_usage.h"
#include "renderer/device.h"
#include "renderer/vma/image.h"
#include "pch.h"

namespace yuubi {

Viewport::Viewport(std::shared_ptr<vk::raii::SurfaceKHR> surface,
                   std::shared_ptr<Device> device)
    : surface_(surface), device_(device) {
    createSwapChain();
    createImageViews();
    createDepthStencil();
    createFrames();
}

Viewport& Viewport::operator=(Viewport&& rhs) {
    // Destroy this (done automatically)

    // Move from rhs to this
    surface_ = rhs.surface_;
    device_ = rhs.device_;
    swapChain_ = std::move(rhs.swapChain_);
    images_ = std::move(rhs.images_);
    swapChainImageFormat_ = rhs.swapChainImageFormat_;
    swapChainExtent_ = rhs.swapChainExtent_;
    depthImage_ = std::move(rhs.depthImage_);
    frames_ = std::move(rhs.frames_);

    // Invalidate rhs
    rhs.device_ = nullptr;
    rhs.surface_ = nullptr;
    rhs.swapChain_ = nullptr;
    rhs.depthImage_ = Image{}; 

    return *this;
}

void Viewport::recreateSwapChain() {
    createSwapChain();
    createImageViews();
    createDepthStencil();
}

void Viewport::createSwapChain() {
    auto surfaceFormat = chooseSwapSurfaceFormat();
    auto presentMode = chooseSwapPresentMode();
    auto swapExtent = chooseSwapExtent();

    const auto capabilities =
        device_->getPhysicalDevice().getSurfaceCapabilitiesKHR(*surface_);
    uint32_t imageCount = capabilities.minImageCount;
    if (capabilities.maxImageCount > 0 &&
        imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    // Create swap chain
    vk::SwapchainCreateInfoKHR createInfo{
        .surface = *surface_,
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = swapExtent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = presentMode,
        .clipped = vk::True,
        .oldSwapchain = nullptr,
    };

    swapChain_ = vk::raii::SwapchainKHR(device_->getDevice(), createInfo);
    swapChainImageFormat_ = surfaceFormat.format;
    swapChainExtent_ = swapExtent;
}

void Viewport::createImageViews() {
    const auto images = swapChain_.getImages();
    images_.clear();
    for (const auto image : images) {
        vk::ImageViewCreateInfo imageViewCreateInfo{
            .image = image,
            .viewType = vk::ImageViewType::e2D,
            .format = swapChainImageFormat_,
            .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                                 .baseMipLevel = 0,
                                 .levelCount = 1,
                                 .baseArrayLayer = 0,
                                 .layerCount = 1},
        };

        SwapchainImage swapchainImage {
            .image = image,
            .imageView = {device_->getDevice(), imageViewCreateInfo}
        };

        images_.push_back(std::move(swapchainImage));
    }
}

void Viewport::createDepthStencil() {
    vk::Format depthFormat = findDepthFormat();

    depthImage_ = device_->createImage(swapChainExtent_.width, swapChainExtent_.height, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal);
    
    depthImageView_ = device_->createImageView(depthImage_.getImage(), depthFormat, vk::ImageAspectFlagBits::eDepth);
}

void Viewport::createFrames() {
    for (auto& frame : frames_) {
        frame.inFlight = vk::raii::Fence{device_->getDevice(), {}};
        frame.imageAvailable = vk::raii::Semaphore{device_->getDevice(), {}};
        frame.renderFinished = vk::raii::Semaphore{device_->getDevice(), {}};

        frame.commandPool = vk::raii::CommandPool{device_->getDevice(), {
            .queueFamilyIndex = device_->getQueue().familyIndex
        }};

        // TODO: wtf?
        vk::CommandBufferAllocateInfo allocInfo {
            .commandPool = frame.commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1 
        };
        frame.commandBuffer = std::move(device_->getDevice().allocateCommandBuffers(allocInfo)[0]);
    }
}

vk::Format Viewport::findDepthFormat() const
{
return findSupportedFormat(
        {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint,
         vk::Format::eD24UnormS8Uint},
        vk::ImageTiling::eOptimal,
        vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

vk::Format Viewport::findSupportedFormat(
    const std::vector<vk::Format>& candidates, vk::ImageTiling tiling,
    vk::FormatFeatureFlags features) const {
    for (vk::Format format : candidates) {
        auto props = device_->getPhysicalDevice().getFormatProperties(format);

        if (tiling == vk::ImageTiling::eLinear &&
            (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == vk::ImageTiling::eOptimal &&
                   (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    UB_ERROR("Failed to find supported format.");
}

vk::SurfaceFormatKHR Viewport::chooseSwapSurfaceFormat() const {
    const auto availableFormats =
        device_->getPhysicalDevice().getSurfaceFormatsKHR(*surface_);
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
            availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return availableFormat;
        }
    }

    return availableFormats.front();
}

vk::PresentModeKHR Viewport::chooseSwapPresentMode() const {
    const auto availablePresentModes =
        device_->getPhysicalDevice().getSurfacePresentModesKHR(*surface_);
    for (const auto& mode : availablePresentModes) {
        if (mode == vk::PresentModeKHR::eMailbox) {
            return mode;
        }
    }
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D Viewport::chooseSwapExtent() const {
    const auto capabilities =
        device_->getPhysicalDevice().getSurfaceCapabilitiesKHR(*surface_);
    if (capabilities.currentExtent.width ==
        std::numeric_limits<uint32_t>::max()) {
        // TODO: implement
        UB_ERROR("Surface extent set to max!");
    }
    return capabilities.currentExtent;
}

bool Viewport::doFrame(std::function<void(const Frame&, const SwapchainImage&)> f)
{
    // Wait for GPU to finish previous work on this frame.
    device_->getDevice().waitForFences(*currentFrame().inFlight, vk::True, std::numeric_limits<uint32_t>::max());

    // Get swapchain image index for this frame.
    uint32_t imageIndex;
    try {
        // This line gets immediately gets the index of the next image in the
        // swapchain. This image may not be available for immediate use, so a
        // semaphore is used to synchronize commands that require this image

        auto [result, idx] = device_->getDevice().acquireNextImage2KHR({
            .swapchain = swapChain_,
            .timeout = std::numeric_limits<uint64_t>::max(),
            .semaphore = currentFrame().imageAvailable
        });
        imageIndex = idx;

    } catch (const std::exception& e) {
        recreateSwapChain();
        return false;
    }

    device_->getDevice().resetFences(*currentFrame().inFlight);

    // Submit commands for rendering this frame.
    f(currentFrame(), images_[imageIndex]);

    // Present this frame.
    vk::PresentInfoKHR presentInfo {
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &(*currentFrame().renderFinished),
        .swapchainCount = 1,
        .pSwapchains = &(*swapChain_),
        .pImageIndices = &imageIndex,
    };

    vk::Result presentResult;
    try {
        presentResult = device_->getQueue().queue.presentKHR(presentInfo);
    } catch (const std::exception& e) {
        frameBufferResized_ = true;
    }

    if (frameBufferResized_ || presentResult == vk::Result::eSuboptimalKHR) {
        frameBufferResized_ = false;
        recreateSwapChain();
    }

    currentFrame_ = (currentFrame_ + 1) % maxFramesInFlight_;
    return true;
}

}