#include "renderer/device.h"
#include "renderer/vulkan_usage.h"
#include "renderer/vma/allocator.h"
#include "renderer/vma/image.h"
#include "renderer/vma/buffer.h"
#include "pch.h"

namespace util {
    uint32_t findGraphicsQueueFamilyIndex(const vk::raii::PhysicalDevice& physicalDevice) {
        for (auto properties = physicalDevice.getQueueFamilyProperties();
             const auto [i, p]: std::views::enumerate(properties)) {
            if (p.queueFlags & vk::QueueFlagBits::eGraphics) {
                return i;
            }
        }
        // TODO: overflow
        UB_ERROR("Cannot find graphics queue family index");
        return -1;
    }
}

namespace yuubi {

    Device::Device(const vk::raii::Instance& instance, const vk::raii::SurfaceKHR& surface) {
        UB_INFO("Creating device...");
        selectPhysicalDevice(instance, surface);
        createLogicalDevice(instance);
        createImmediateCommandResources();
        UB_INFO("Created device...");
    }

    void Device::selectPhysicalDevice(const vk::raii::Instance& instance, const vk::raii::SurfaceKHR& surface) {
        vk::raii::PhysicalDevices physicalDevices{instance};

        std::ranges::partition(physicalDevices, [&](const auto& device) {
            return device.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu;
        });

        auto suitableDeviceIter = std::ranges::find_if(physicalDevices, [this, &surface](const auto& device) {
            return isDeviceSuitable(device, surface);
        });

        if (suitableDeviceIter == physicalDevices.end()) {
            UB_ERROR("Failed to find a suitable GPU!");
        }

        // TODO: Is this correct? Do I move here?
        physicalDevice_ = std::move(*suitableDeviceIter);
    }

    bool Device::isDeviceSuitable(const vk::raii::PhysicalDevice& physicalDevice, const vk::raii::SurfaceKHR& surface) {
        bool isGraphicsCapable = false;
        std::vector<vk::QueueFamilyProperties> properties = physicalDevice.getQueueFamilyProperties();

        for (const auto [i, p]: std::views::enumerate(properties)) {
            if (p.queueFlags & vk::QueueFlagBits::eGraphics && physicalDevice.getSurfaceSupportKHR(i, *surface)) {
                isGraphicsCapable = true;
                break;
            }
        }

        return isGraphicsCapable && supportsFeatures(physicalDevice);
    }

    bool Device::supportsFeatures(const vk::raii::PhysicalDevice& physicalDevice) {
        auto supportedFeatures = physicalDevice.getFeatures2<
            vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features, vk::PhysicalDeviceVulkan12Features,
            vk::PhysicalDeviceVulkan13Features>();

        // TODO: compare all features
        auto availableFeatures11 = supportedFeatures.get<vk::PhysicalDeviceVulkan11Features>();
        auto requiredFeatures11 = requiredFeatures_.get<vk::PhysicalDeviceVulkan11Features>();

        auto availableFeatures12 = supportedFeatures.get<vk::PhysicalDeviceVulkan12Features>();
        auto requiredFeatures12 = requiredFeatures_.get<vk::PhysicalDeviceVulkan12Features>();
        if (requiredFeatures12.descriptorIndexing && !availableFeatures12.descriptorIndexing) {
            return false;
        }
        if (requiredFeatures12.bufferDeviceAddress && !availableFeatures12.bufferDeviceAddress) {
            return false;
        }

        auto availableFeatures13 = supportedFeatures.get<vk::PhysicalDeviceVulkan13Features>();
        auto requiredFeatures13 = requiredFeatures_.get<vk::PhysicalDeviceVulkan13Features>();
        if (requiredFeatures13.dynamicRendering && !availableFeatures13.dynamicRendering) {
            return false;
        }
        if (requiredFeatures13.synchronization2 && !availableFeatures13.synchronization2) {
            return false;
        }

        return true;
    }

    void Device::createLogicalDevice(const vk::raii::Instance& instance) {
        constexpr float priority = 1.0f;
        const auto graphicsFamilyIndex = util::findGraphicsQueueFamilyIndex(physicalDevice_);

        const vk::DeviceQueueCreateInfo queueCreateInfo{
            .queueFamilyIndex = graphicsFamilyIndex, .queueCount = 1, .pQueuePriorities = &priority
        };

        const vk::DeviceCreateInfo createInfo{
            .pNext = &requiredFeatures_.get(),
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queueCreateInfo,
            .enabledLayerCount = 0,
            .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions_.size()),
            .ppEnabledExtensionNames = requiredExtensions_.data()
        };

        device_ = vk::raii::Device{physicalDevice_, createInfo};

        graphicsQueue_ = {
            .queue = device_.getQueue2({.queueFamilyIndex = graphicsFamilyIndex, .queueIndex = 0}),
            .familyIndex = graphicsFamilyIndex
        };

        allocator_ = std::make_shared<Allocator>(instance, physicalDevice_, device_);
    }

    vk::raii::ImageView Device::createImageView(
        const vk::Image& image, const vk::Format& format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels,
        const vk::ImageViewType type
    ) const {
        // TODO: this looks ugly
        const uint32_t numLayers = type == vk::ImageViewType::eCube ? 6 : 1;

        vk::ImageViewCreateInfo const viewInfo{
            .image = image,
            .viewType = type,
            .format = format,
            .subresourceRange = {
                                 .aspectMask = aspectFlags,
                                 .baseMipLevel = 0,
                                 .levelCount = mipLevels,
                                 .baseArrayLayer = 0,
                                 .layerCount = numLayers,
                                 },
        };

        return device_.createImageView(viewInfo);
    }

    Image Device::createImage(const ImageCreateInfo& createInfo) const { return Image{allocator_.get(), createInfo}; }

    Buffer Device::createBuffer(
        const vk::BufferCreateInfo& createInfo, const vma::AllocationCreateInfo& allocInfo
    ) const {
        return Buffer{allocator_.get(), createInfo, allocInfo};
    }

    void Device::createImmediateCommandResources() {
        immediateCommandPool_ = vk::raii::CommandPool{
            device_,
            {.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
              .queueFamilyIndex = graphicsQueue_.familyIndex}
        };

        // TODO: wtf?
        vk::CommandBufferAllocateInfo const allocInfo{
            .commandPool = *immediateCommandPool_, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1
        };
        immediateCommandBuffer_ = std::move(device_.allocateCommandBuffers(allocInfo)[0]);

        immediateCommandFence_ =
            vk::raii::Fence{device_, vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled}};
    }

    // PERF: Immediate commands are typically used to load data onto the GPU,
    // blocking the main thread. Handle these operations asynchronously instead
    // via C++20 coroutines.
    void Device::submitImmediateCommands(
        const std::function<void(const vk::raii::CommandBuffer& commandBuffer)>& function
    ) const {
        device_.resetFences(*immediateCommandFence_);
        immediateCommandBuffer_.reset();

        immediateCommandBuffer_.begin(
            vk::CommandBufferBeginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit}
        );

        function(immediateCommandBuffer_);

        immediateCommandBuffer_.end();

        vk::CommandBufferSubmitInfo const commandBufferSubmitInfo{
            .commandBuffer = *immediateCommandBuffer_,
        };
        graphicsQueue_.queue.submit2(
            {
                vk::SubmitInfo2{
                                .commandBufferInfoCount = 1,
                                .pCommandBufferInfos = &commandBufferSubmitInfo,
                                }
        },
            *immediateCommandFence_
        );

        device_.waitForFences({*immediateCommandFence_}, vk::True, std::numeric_limits<uint64_t>::max());
    }

    const vk::StructureChain<
        vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features, vk::PhysicalDeviceVulkan12Features,
        vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceDynamicRenderingLocalReadFeaturesKHR>
        Device::requiredFeatures_{
            vk::PhysicalDeviceFeatures2{.features = {.multiViewport = vk::True, .samplerAnisotropy = vk::True}},
            vk::PhysicalDeviceVulkan11Features{.multiview = vk::True},
            vk::PhysicalDeviceVulkan12Features{
                                        .descriptorIndexing = vk::True,
                                        .shaderSampledImageArrayNonUniformIndexing = vk::True,
                                        .descriptorBindingSampledImageUpdateAfterBind = vk::True,
                                        .descriptorBindingUpdateUnusedWhilePending = vk::True,
                                        .descriptorBindingPartiallyBound = vk::True,
                                        .descriptorBindingVariableDescriptorCount = vk::True,
                                        .runtimeDescriptorArray = vk::True,
                                        .scalarBlockLayout = vk::True,
                                        .hostQueryReset = vk::True,
                                        .bufferDeviceAddress = vk::True,
                                        },
            vk::PhysicalDeviceVulkan13Features{.synchronization2 = vk::True, .dynamicRendering = vk::True},
            vk::PhysicalDeviceDynamicRenderingLocalReadFeaturesKHR{.dynamicRenderingLocalRead = vk::True}
    };

    const std::vector<const char*> Device::requiredExtensions_{
        vk::KHRSwapchainExtensionName, vk::KHRDynamicRenderingLocalReadExtensionName
    };

}
