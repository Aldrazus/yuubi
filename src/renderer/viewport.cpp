#include "renderer/viewport.h"

#include <GLFW/glfw3.h>
#include "renderer/vulkan_usage.h"
#include "renderer/device.h"
#include "renderer/vma/image.h"

namespace yuubi {

Viewport::Viewport(std::shared_ptr<vk::raii::SurfaceKHR> surface,
                   std::shared_ptr<Device> device)
    : surface_(surface), device_(device) {
    createSwapChain();
    createImageViews();
    createDepthStencil();
}

Viewport& Viewport::operator=(Viewport&& rhs) {
    surface_ = rhs.surface_;
    rhs.surface_ = nullptr;

    device_ = rhs.device_;
    rhs.device_ = nullptr;

    swapChain_ = std::move(rhs.swapChain_);
    rhs.swapChain_ = nullptr;

    imageViews_ = std::move(rhs.imageViews_);
    swapChainImageFormat_ = rhs.swapChainImageFormat_;
    swapChainExtent_ = rhs.swapChainExtent_;

    depthImage_ = std::move(rhs.depthImage_);
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
    // Create image views
    const auto images = swapChain_.getImages();
    imageViews_.clear();
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

        imageViews_.emplace_back(device_->getDevice(), imageViewCreateInfo);
    }
}

void Viewport::createDepthStencil() {
    vk::Format depthFormat = findDepthFormat();

    depthImage_ = device_->createImage(swapChainExtent_.width, swapChainExtent_.height, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal);
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

}
