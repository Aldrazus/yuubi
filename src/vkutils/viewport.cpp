#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "vkutils/vulkan_usage.h"

#include "vkutils/viewport.h"

namespace yuubi {
namespace vkutils {

Viewport::Viewport(vk::SurfaceKHR surface, vk::PhysicalDevice physicalDevice, vk::Device device, vk::Queue presentQueue) : surface_(surface), physicalDevice_(physicalDevice), device_(device), presentQueue_(presentQueue) {}

void Viewport::initialize() {
    createSwapChain();
    createImageViews();
    createDepthStencil();
    createSyncObjects();
}

void Viewport::destroySwapChain() {
    // Destroy depth image
    device_.destroyImageView(depthImageView_);
    device_.destroyImage(depthImage_);
    device_.freeMemory(depthImageMemory_);

    // Destroy swap chain image views
    for (uint32_t i = 0; i < swapChainImageViews_.size(); i++) {
        device_.destroyImageView(swapChainImageViews_[i]);
    }
    swapChainImageViews_.clear();

    // Destroy swap chain
    device_.destroySwapchainKHR(swapChain_);
}

void Viewport::destroy() {
    destroySwapChain();

    for (uint32_t i = 0; i < imageAvailableSemaphores_.size(); i++) {
        device_.destroySemaphore(imageAvailableSemaphores_[i]);
        device_.destroySemaphore(renderFinishedSemaphores_[i]);
        device_.destroyFence(inFlightFences_[i]);
    }
}

void Viewport::createSwapChain() {
    auto swapChainSupport = querySwapChainSupport(physicalDevice_);

    auto surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    auto presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    auto swapExtent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount;
    if (swapChainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR createInfo{
        .surface = surface_,
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = swapExtent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
        .preTransform = swapChainSupport.capabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = presentMode,
        .clipped = vk::True,
        .oldSwapchain = nullptr};

    // XXX: assumes graphics and present indices are the same
    createInfo.imageSharingMode = vk::SharingMode::eExclusive;
    createInfo.queueFamilyIndexCount = 0;      // Optional
    createInfo.pQueueFamilyIndices = nullptr;  // Optional

    swapChain_ = device_.createSwapchainKHR(createInfo);
    swapChainImages_ = device_.getSwapchainImagesKHR(swapChain_);
    swapChainImageFormat_ = surfaceFormat.format;
    swapChainExtent_ = swapExtent;
}

void Viewport::recreateSwapChain() {
    device_.waitIdle();

    destroySwapChain();

    createSwapChain();
    createImageViews();
    createDepthStencil();
}

void Viewport::createDepthStencil() {
    depthFormat_ = findDepthFormat();

    createImage(swapChainExtent_.width, swapChainExtent_.height, depthFormat_,
                vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eDepthStencilAttachment,
                vk::MemoryPropertyFlagBits::eDeviceLocal, depthImage_,
                depthImageMemory_);

    depthImageView_ = createImageView(depthImage_, depthFormat_,
                                      vk::ImageAspectFlagBits::eDepth);
}

vk::Format Viewport::findDepthFormat() {
    return findSupportedFormat(
        {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint,
         vk::Format::eD24UnormS8Uint},
        vk::ImageTiling::eOptimal,
        vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

vk::Format Viewport::findSupportedFormat(
    const std::vector<vk::Format>& candidates, vk::ImageTiling tiling,
    vk::FormatFeatureFlags features) {
    for (vk::Format format : candidates) {
        auto props = physicalDevice_.getFormatProperties(format);

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

Viewport::SwapChainSupportDetails Viewport::querySwapChainSupport(
    vk::PhysicalDevice physicalDevice) const {
    return {.capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface_),
            .formats = physicalDevice.getSurfaceFormatsKHR(surface_),
            .presentModes = physicalDevice.getSurfacePresentModesKHR(surface_)};
}

vk::SurfaceFormatKHR Viewport::chooseSwapSurfaceFormat(
    const std::vector<vk::SurfaceFormatKHR>& availableFormats) const {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
            availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return availableFormat;
        }
    }

    return availableFormats.front();
}

vk::PresentModeKHR Viewport::chooseSwapPresentMode(
    const std::vector<vk::PresentModeKHR>& availablePresentModes) const {
    for (const auto& mode : availablePresentModes) {
        if (mode == vk::PresentModeKHR::eMailbox) {
            return mode;
        }
    }
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D Viewport::chooseSwapExtent(
    const vk::SurfaceCapabilitiesKHR& capabilities) const {
    if (capabilities.currentExtent.width !=
        std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    // TODO: account for this
    /*
    int width, height;
    glfwGetFramebufferSize(window_.getWindow(), &width, &height);
    vk::Extent2D extent = {
        .width = static_cast<uint32_t>(width),
        .height = static_cast<uint32_t>(height),
    };

    extent.width = std::clamp(extent.width, capabilities.minImageExtent.width,
                              capabilities.maxImageExtent.width);
    extent.height =
        std::clamp(extent.height, capabilities.minImageExtent.height,
                   capabilities.maxImageExtent.height);
    return extent;
    */
}

void Viewport::createImageViews() {
    for (const auto& image : swapChainImages_) {
        swapChainImageViews_.push_back(createImageView(
            image, swapChainImageFormat_, vk::ImageAspectFlagBits::eColor));
    }
}

vk::ImageView Viewport::createImageView(vk::Image image, vk::Format format,
                                        vk::ImageAspectFlags aspectFlags) {
    vk::ImageViewCreateInfo viewInfo{
        .image = image,
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .subresourceRange =
            {
                .aspectMask = aspectFlags,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    return device_.createImageView(viewInfo);
}

void Viewport::createImage(uint32_t width, uint32_t height, vk::Format format,
                           vk::ImageTiling tiling, vk::ImageUsageFlags usage,
                           vk::MemoryPropertyFlags properties, vk::Image& image,
                           vk::DeviceMemory& imageMemory) {
    vk::ImageCreateInfo imageInfo{
        .imageType = vk::ImageType::e2D,
        .format = format,
        .extent = {.width = width, .height = height, .depth = 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling = tiling,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive,
        .initialLayout = vk::ImageLayout::eUndefined,
    };

    image = device_.createImage(imageInfo);

    vk::MemoryRequirements memRequirements =
        device_.getImageMemoryRequirements(image);

    vk::MemoryAllocateInfo allocInfo{
        .allocationSize = memRequirements.size,
        .memoryTypeIndex =
            findMemoryType(memRequirements.memoryTypeBits, properties)};

    imageMemory = device_.allocateMemory(allocInfo);
    device_.bindImageMemory(image, imageMemory, 0);
}

uint32_t Viewport::findMemoryType(uint32_t typeFilter,
                                  vk::MemoryPropertyFlags properties) {
    auto memProperties = physicalDevice_.getMemoryProperties();

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        // Bit i is set if and only if the memory type i in the
        // VkPhysicalDeviceMemoryProperties structure for the physical device is
        // supported for the resource.
        // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkMemoryRequirements.html
        if (typeFilter & (1 << i) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) ==
                properties) {
            return i;
        }
    }

    UB_ERROR("Unable to find suitable memory type");
    throw std::runtime_error("Failed to find suitable memory type");
}

void Viewport::createSyncObjects() {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        imageAvailableSemaphores_.push_back(device_.createSemaphore({}));
        renderFinishedSemaphores_.push_back(device_.createSemaphore({}));
        inFlightFences_.push_back(
            device_.createFence({.flags = vk::FenceCreateFlagBits::eSignaled}));
    }
}

bool Viewport::beginFrame() {
    // Wait until the previous frame is done rendering
    device_.waitForFences(1, &inFlightFences_[currentFrame_], vk::True, std::numeric_limits<uint32_t>::max());

    // TODO: remove exceptions
    try {
        // This line gets immediately gets the index of the next image in the
        // swapchain. This image may not be available for immediate use, so a
        // semaphore is used to synchronize commands that require this image

        // TODO: ERROR HERE! Swapchain is destroyed on move in constructor!!!
        auto [result, idx] = device_.acquireNextImageKHR(
            swapChain_, std::numeric_limits<uint64_t>::max(),
            imageAvailableSemaphores_[currentFrame_]);
        imageIndex_ = idx;
    } catch (const std::exception& e) {
        recreateSwapChain();
        return false;
    }

    auto resetFenceResult =
        device_.resetFences(1, &inFlightFences_[currentFrame_]);

    return true;
}

bool Viewport::endFrame() {
    vk::PresentInfoKHR presentInfo{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderFinishedSemaphores_[currentFrame_],
        .swapchainCount = 1,
        .pSwapchains = &swapChain_,
        .pImageIndices = &imageIndex_,
    };

    vk::Result presentResult;
    // TODO: remove exceptions
    try {
        presentResult = presentQueue_.presentKHR(presentInfo);
    } catch (const std::exception& e) {
        UB_INFO(e.what());
        framebufferResized_ = true;
    }

    if (framebufferResized_ || presentResult == vk::Result::eSuboptimalKHR) {
        framebufferResized_ = false;
        recreateSwapChain();
    }

    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;

    return true;
}


} // namespace vkutils
} // namespace yuubi
