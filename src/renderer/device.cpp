#include "renderer/device.h"
#include "renderer/vma/allocator.h"
#include "renderer/vma/image.h"

namespace util {
uint32_t findGraphicsQueueFamilyIndex(
    const vk::raii::PhysicalDevice& physicalDevice) {
    auto properties = physicalDevice.getQueueFamilyProperties();

    for (const auto [i, p] : std::views::enumerate(properties)) {
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

Device::Device(const vk::raii::Instance& instance,
               const vk::raii::SurfaceKHR& surface) {
    selectPhysicalDevice(instance, surface);
    createLogicalDevice(instance);
}

void Device::selectPhysicalDevice(const vk::raii::Instance& instance,
                                  const vk::raii::SurfaceKHR& surface) {
    vk::raii::PhysicalDevices physicalDevices{instance};
    std::partition(physicalDevices.begin(), physicalDevices.end(),
                   [](const vk::raii::PhysicalDevice& device) {
                       return device.getProperties().deviceType ==
                              vk::PhysicalDeviceType::eDiscreteGpu;
                   });

    auto suitableDeviceIter =
        std::find_if(physicalDevices.begin(), physicalDevices.end(),
                     [this, &surface](const vk::raii::PhysicalDevice device) {
                         return isDeviceSuitable(device, surface);
                     });

    if (suitableDeviceIter == physicalDevices.end()) {
        UB_ERROR("Failed to find a suitable GPU!");
    }

    // TODO: Is this correct? Do I move here?
    physicalDevice_ = std::move(*suitableDeviceIter);
}

bool Device::isDeviceSuitable(const vk::raii::PhysicalDevice& physicalDevice,
                              const vk::raii::SurfaceKHR& surface) {
    bool isGraphicsCapable = false;
    std::vector<vk::QueueFamilyProperties> properties =
        physicalDevice.getQueueFamilyProperties();

    for (const auto [i, p] : std::views::enumerate(properties)) {
        if (p.queueFlags & vk::QueueFlagBits::eGraphics &&
            physicalDevice.getSurfaceSupportKHR(i, surface)) {
            isGraphicsCapable = true;
            break;
        }
    }

    return isGraphicsCapable && supportsFeatures(physicalDevice);
}

bool Device::supportsFeatures(const vk::raii::PhysicalDevice& physicalDevice) {
    auto supportedFeatures =
        physicalDevice.getFeatures2<vk::PhysicalDeviceFeatures2,
                                    vk::PhysicalDeviceVulkan11Features,
                                    vk::PhysicalDeviceVulkan12Features,
                                    vk::PhysicalDeviceVulkan13Features>();

    // TODO: compare all features
    auto availableFeatures11 =
        supportedFeatures.get<vk::PhysicalDeviceVulkan11Features>();
    auto requiredFeatures11 =
        requiredFeatures_.get<vk::PhysicalDeviceVulkan11Features>();

    auto availableFeatures12 =
        supportedFeatures.get<vk::PhysicalDeviceVulkan12Features>();
    auto requiredFeatures12 =
        requiredFeatures_.get<vk::PhysicalDeviceVulkan12Features>();
    if (requiredFeatures12.bufferDeviceAddress &&
        !availableFeatures12.bufferDeviceAddress) {
        return false;
    }
    if (requiredFeatures12.descriptorIndexing &&
        !availableFeatures12.descriptorIndexing)
        return false;

    auto availableFeatures13 =
        supportedFeatures.get<vk::PhysicalDeviceVulkan13Features>();
    auto requiredFeatures13 =
        requiredFeatures_.get<vk::PhysicalDeviceVulkan13Features>();
    if (requiredFeatures13.dynamicRendering &&
        !availableFeatures13.dynamicRendering)
        return false;
    if (requiredFeatures13.synchronization2 &&
        !availableFeatures13.synchronization2)
        return false;

    return true;
}

void Device::createLogicalDevice(const vk::raii::Instance& instance) {
    float priority = 1.0f;
    const auto graphicsFamilyIndex =
        util::findGraphicsQueueFamilyIndex(physicalDevice_);

    vk::DeviceQueueCreateInfo queueCreateInfo{
        .queueFamilyIndex = graphicsFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &priority};

    vk::DeviceCreateInfo createInfo{
        .pNext = &requiredFeatures_,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledLayerCount = 0,
        .enabledExtensionCount =
            static_cast<uint32_t>(requiredExtensions_.size()),
        .ppEnabledExtensionNames = requiredExtensions_.data()};

    device_ = vk::raii::Device{physicalDevice_, createInfo};

    graphicsQueue_ = {
        .queue = device_.getQueue2(
            {.queueFamilyIndex = graphicsFamilyIndex, .queueIndex = 0}),
        .familyIndex = graphicsFamilyIndex};

    allocator_ = std::make_shared<Allocator>(instance, physicalDevice_, device_);
}

Image Device::createImage(uint32_t width, uint32_t height, vk::Format format,
                          vk::ImageTiling tiling, vk::ImageUsageFlags usage,
                          vk::MemoryPropertyFlags properties) {
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

    return Image{allocator_, imageInfo};
}

Buffer Device::createBuffer(const vk::BufferCreateInfo& createInfo, const vma::AllocationCreateInfo& allocInfo)
{
    return Buffer{allocator_, createInfo, allocInfo};
}

vk::raii::ImageView Device::createImageView(const vk::Image& image, const vk::Format& format, vk::ImageAspectFlags aspectFlags)
{
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

const vk::StructureChain<
    vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
    vk::PhysicalDeviceVulkan12Features, vk::PhysicalDeviceVulkan13Features>
    Device::requiredFeatures_{
        vk::PhysicalDeviceFeatures2{},
        vk::PhysicalDeviceVulkan11Features{},
        vk::PhysicalDeviceVulkan12Features{.descriptorIndexing = vk::True, .bufferDeviceAddress = vk::True, .bufferDeviceAddressCaptureReplay = vk::True},
        vk::PhysicalDeviceVulkan13Features{.synchronization2 = vk::True, .dynamicRendering = vk::True},
};

const std::vector<const char*> Device::requiredExtensions_{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

}
