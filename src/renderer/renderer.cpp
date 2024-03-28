#include <cstring>
#include <limits>

#include "vkutils/viewport.h"
#include "vkutils/vulkan_usage.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <chrono>

#include "renderer/renderer.h"
#include "pch.h"

#define MAX_FRAMES_IN_FLIGHT 2

vk::VertexInputBindingDescription Vertex::getBindingDescription() {
    vk::VertexInputBindingDescription bindingDescription{
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = vk::VertexInputRate::eVertex};

    return bindingDescription;
}

std::array<vk::VertexInputAttributeDescription, 3>
Vertex::getAttributeDescriptions() {
    std::array<vk::VertexInputAttributeDescription, 3> attributeDescriptions;
    attributeDescriptions[0] = {.location = 0,
                                .binding = 0,
                                .format = vk::Format::eR32G32B32Sfloat,
                                .offset = offsetof(Vertex, pos)};

    attributeDescriptions[1] = {.location = 1,
                                .binding = 0,
                                .format = vk::Format::eR32G32B32Sfloat,
                                .offset = offsetof(Vertex, color)};

    attributeDescriptions[2] = {.location = 2,
                                .binding = 0,
                                .format = vk::Format::eR32G32Sfloat,
                                .offset = offsetof(Vertex, texCoord)};
    return attributeDescriptions;
}

Renderer::Renderer(const Window& window) : window_(window) {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();

    viewport_ = yuubi::vkutils::Viewport(surface_, physicalDevice_, device_, presentQueue_);
    viewport_.initialize();

    createDescriptorSetLayout();
    createGraphicsPipeline();
    createCommandPool();
    createTextureImage();
    createTextureImageView();
    createTextureSampler();
    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();
}

Renderer::~Renderer() {
    device_.waitIdle();

    device_.destroySampler(textureSampler_);
    device_.destroyImageView(textureImageView_, nullptr);
    device_.destroyImage(textureImage_);
    device_.freeMemory(textureImageMemory_);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        device_.destroyBuffer(uniformBuffers_[i]);
        device_.freeMemory(uniformBuffersMemory_[i]);
    }

    device_.destroyDescriptorPool(descriptorPool_);
    device_.destroyDescriptorSetLayout(descriptorSetLayout_);

    device_.destroyBuffer(vertexBuffer_);
    device_.freeMemory(vertexBufferMemory_);
    device_.destroyBuffer(indexBuffer_);
    device_.freeMemory(indexBufferMemory_);

    device_.destroyPipeline(graphicsPipeline_);
    device_.destroyPipelineLayout(pipelineLayout_);

    device_.destroyCommandPool(commandPool_);

    viewport_.destroy();

    device_.destroy();
    if (enableValidationLayers_) {
        instance_.destroyDebugUtilsMessengerEXT(debugMessenger_, nullptr,
                                                dldi_);
    }
    instance_.destroySurfaceKHR(surface_);
    instance_.destroy();
}

void Renderer::createSurface() {
    VkSurfaceKHR tmp;

    if (glfwCreateWindowSurface(static_cast<VkInstance>(instance_),
                                window_.getWindow(), nullptr,
                                &tmp) != VK_SUCCESS) {
        UB_ERROR("Unable to create window surface!");
    }

    surface_ = vk::SurfaceKHR(tmp);
}

void Renderer::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);

    float priority = 1.0f;
    vk::DeviceQueueCreateInfo queueCreateInfo{
        .queueFamilyIndex = indices.graphicsFamily.value(),
        .queueCount = 1,
        .pQueuePriorities = &priority,
    };

    vk::PhysicalDeviceFeatures deviceFeatures{.samplerAnisotropy = vk::True};

    vk::PhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeature{
        .dynamicRendering = vk::True
    };

    vk::DeviceCreateInfo createInfo{
        .pNext = &dynamicRenderingFeature,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledLayerCount = 0,
        .enabledExtensionCount =
            static_cast<uint32_t>(deviceExtensions_.size()),
        .ppEnabledExtensionNames = deviceExtensions_.data(),
        .pEnabledFeatures = &deviceFeatures,
    };

    if (enableValidationLayers_) {
        createInfo.enabledLayerCount = validationLayers_.size();
        createInfo.ppEnabledLayerNames = validationLayers_.data();
    }
    device_ = physicalDevice_.createDevice(createInfo);
    graphicsQueue_ = device_.getQueue(indices.graphicsFamily.value(), 0);
    presentQueue_ = device_.getQueue(indices.presentFamily.value(), 0);
    dldy_.init(instance_, vkGetInstanceProcAddr, device_, vkGetDeviceProcAddr);
}

void Renderer::createInstance() {
    if (enableValidationLayers_ && !checkValidationLayerSupport()) {
        UB_ERROR("Validation layers requested, but unavailable!");
    }
    vk::ApplicationInfo appInfo{
        .pApplicationName = "Yuubi",
        .applicationVersion = vk::makeApiVersion(0, 1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = vk::makeApiVersion(0, 1, 0, 0),
        .apiVersion = vk::ApiVersion10};

    auto extensions = getRequiredExtensions();

    vk::InstanceCreateInfo createInfo{
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = 0,
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };

    if (enableValidationLayers_) {
        createInfo.enabledLayerCount = validationLayers_.size();
        createInfo.ppEnabledLayerNames = validationLayers_.data();

        vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo{
            .messageSeverity =
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning,
            .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                           vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                           vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
            .pfnUserCallback = debugCallback,
            .pUserData = nullptr};
        createInfo.pNext = &debugCreateInfo;
    }

    instance_ = vk::createInstance(createInfo);
}

void Renderer::pickPhysicalDevice() {
    std::vector<vk::PhysicalDevice> physicalDevices =
        instance_.enumeratePhysicalDevices();

    auto suitableDeviceIter = std::find_if(
        physicalDevices.begin(), physicalDevices.end(),
        [this](vk::PhysicalDevice device) { return isDeviceSuitable(device); });

    if (suitableDeviceIter == physicalDevices.end()) {
        UB_ERROR("Failed to find a suitable GPU!");
    }

    physicalDevice_ = *suitableDeviceIter;
}

QueueFamilyIndices Renderer::findQueueFamilies(
    vk::PhysicalDevice physicalDevice) {
    QueueFamilyIndices indices;

    std::vector<vk::QueueFamilyProperties> properties =
        physicalDevice.getQueueFamilyProperties();

    for (const auto [i, p] : std::views::enumerate(properties)) {
        if (p.queueFlags & vk::QueueFlagBits::eGraphics &&
            physicalDevice.getSurfaceSupportKHR(i, surface_)) {
            indices.graphicsFamily = indices.presentFamily = i;
        }
    }

    if (indices.isComplete()) {
        return indices;
    }

    for (const auto [i, p] : std::views::enumerate(properties)) {
        if (p.queueFlags & vk::QueueFlagBits::eGraphics) {
            indices.graphicsFamily = i;
        }
    }

    for (const auto [i, p] : std::views::enumerate(properties)) {
        if (physicalDevice.getSurfaceSupportKHR(i, surface_)) {
            indices.presentFamily = i;
        }
    }

    return indices;
}

bool Renderer::isDeviceSuitable(vk::PhysicalDevice physicalDevice) {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    bool extensionsSupported = checkDeviceExtensionSupport(physicalDevice);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        auto swapChainSupport = querySwapChainSupport(physicalDevice);
        swapChainAdequate = !swapChainSupport.formats.empty() &&
                            !swapChainSupport.presentModes.empty();
    }

    auto supportedFeatures = physicalDevice.getFeatures();

    return indices.isComplete() && extensionsSupported && swapChainAdequate &&
           supportedFeatures.samplerAnisotropy;
}

bool Renderer::checkDeviceExtensionSupport(vk::PhysicalDevice device) {
    auto extensionProperties = device.enumerateDeviceExtensionProperties();
    return std::all_of(
        deviceExtensions_.begin(), deviceExtensions_.end(),
        [&extensionProperties](const std::string& extension) {
            return std::find_if(extensionProperties.begin(),
                                extensionProperties.end(),
                                [&extension](const vk::ExtensionProperties& p) {
                                    return std::strcmp(p.extensionName.data(),
                                                       extension.data());
                                }) != extensionProperties.end();
        });
}

bool Renderer::checkValidationLayerSupport() {
    std::vector<vk::LayerProperties> availableLayers =
        vk::enumerateInstanceLayerProperties();

    // TODO: find cleaner approach
    return std::all_of(
        validationLayers_.begin(), validationLayers_.end(),
        [&availableLayers](const std::string& layer) {
            return std::find_if(
                       availableLayers.begin(), availableLayers.end(),
                       [&layer](const vk::LayerProperties& availableLayer) {
                           return std::strcmp(availableLayer.layerName.data(),
                                              layer.data());
                       }) != availableLayers.end();
        });
}

std::vector<const char*> Renderer::getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions,
                                        glfwExtensions + glfwExtensionCount);

    if (enableValidationLayers_) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    return extensions;
}

void Renderer::setupDebugMessenger() {
    if (!enableValidationLayers_) {
        return;
    }

    vk::DebugUtilsMessengerCreateInfoEXT createInfo{
        .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                           vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                           vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning,
        .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                       vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                       vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
        .pfnUserCallback = debugCallback,
        .pUserData = nullptr};

    dldi_.init(instance_, vkGetInstanceProcAddr);
    debugMessenger_ =
        instance_.createDebugUtilsMessengerEXT(createInfo, nullptr, dldi_);
    // TODO:
    /*
    device = physicalDevice.createDevice(..., allocator, dldy);
    dldy.init(device);
    */
}

SwapChainSupportDetails Renderer::querySwapChainSupport(
    vk::PhysicalDevice physicalDevice) {
    return {.capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface_),
            .formats = physicalDevice.getSurfaceFormatsKHR(surface_),
            .presentModes = physicalDevice.getSurfacePresentModesKHR(surface_)};
}

vk::SurfaceFormatKHR Renderer::chooseSwapSurfaceFormat(
    const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
            availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return availableFormat;
        }
    }

    return availableFormats.front();
}

vk::PresentModeKHR Renderer::chooseSwapPresentMode(
    const std::vector<vk::PresentModeKHR>& availablePresentModes) {
    for (const auto& mode : availablePresentModes) {
        if (mode == vk::PresentModeKHR::eMailbox) {
            return mode;
        }
    }
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D Renderer::chooseSwapExtent(
    const vk::SurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width !=
        std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

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
}

void Renderer::createDescriptorSetLayout() {
    vk::DescriptorSetLayoutBinding uboLayoutBinding{
        .binding = 0,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eVertex};

    vk::DescriptorSetLayoutBinding samplerLayoutBinding{
        .binding = 1,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eFragment,
        .pImmutableSamplers = nullptr,
    };

    std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {
        uboLayoutBinding, samplerLayoutBinding};

    vk::DescriptorSetLayoutCreateInfo layoutInfo{
        .bindingCount = bindings.size(), .pBindings = bindings.data()};

    descriptorSetLayout_ = device_.createDescriptorSetLayout(layoutInfo);
}

void Renderer::createGraphicsPipeline() {
    auto vertShaderCode = readFile("shaders/vert.spv");
    auto fragShaderCode = readFile("shaders/frag.spv");

    vk::ShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    vk::ShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
        .stage = vk::ShaderStageFlagBits::eVertex,
        .module = vertShaderModule,
        .pName = "main"};

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
        .stage = vk::ShaderStageFlagBits::eFragment,
        .module = fragShaderModule,
        .pName = "main"};

    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                        fragShaderStageInfo};

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDescription,
        .vertexAttributeDescriptionCount =
            static_cast<uint32_t>(attributeDescriptions.size()),
        .pVertexAttributeDescriptions = attributeDescriptions.data()};

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
        .topology = vk::PrimitiveTopology::eTriangleList,
        .primitiveRestartEnable = vk::False};

    std::vector<vk::DynamicState> dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
    };

    vk::PipelineDynamicStateCreateInfo dynamicState{
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()};

    vk::PipelineViewportStateCreateInfo viewportState{
        .viewportCount = 1,
        .scissorCount = 1,
    };

    vk::PipelineRasterizationStateCreateInfo rasterizer{
        .depthClampEnable = vk::False,
        .rasterizerDiscardEnable = vk::False,
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eBack,
        .frontFace = vk::FrontFace::eCounterClockwise,
        .depthBiasEnable = vk::False,
        .lineWidth = 1.0f,
    };

    vk::PipelineMultisampleStateCreateInfo multisampling{
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
        .sampleShadingEnable = vk::False,
        .minSampleShading = 1.0f,
    };

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable = vk::False,
        .colorWriteMask =
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
    };

    vk::PipelineColorBlendStateCreateInfo colorBlending{
        .logicOpEnable = vk::False,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment};

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
        .setLayoutCount = 1, .pSetLayouts = &descriptorSetLayout_};

    pipelineLayout_ = device_.createPipelineLayout(pipelineLayoutInfo);

    vk::PipelineDepthStencilStateCreateInfo depthStencil{
        .depthTestEnable = vk::True,
        .depthWriteEnable = vk::True,
        .depthCompareOp = vk::CompareOp::eLess,
        .depthBoundsTestEnable = vk::False,
        .stencilTestEnable = vk::False,
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,
    };

    const vk::Format colorFormat = viewport_.getSwapChainImageFormat();
    const vk::Format depthFormat = viewport_.getDepthFormat();

    vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo {
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &colorFormat,
        .depthAttachmentFormat = depthFormat,
    };

    vk::GraphicsPipelineCreateInfo pipelineInfo{
        .pNext = &pipelineRenderingCreateInfo,
        .stageCount = 2,
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = pipelineLayout_,
        .subpass = 0};

    auto [result, pipeline] =
        device_.createGraphicsPipeline(nullptr, pipelineInfo);
    switch (result) {
        case vk::Result::eSuccess:
            graphicsPipeline_ = pipeline;
            break;
        case vk::Result::ePipelineCompileRequiredEXT:
            // TODO: handle error
            break;
        default:
            assert(false);  // should never happen
    }

    device_.destroyShaderModule(vertShaderModule);
    device_.destroyShaderModule(fragShaderModule);
}

void Renderer::createCommandPool() {
    QueueFamilyIndices queueFamilyIndicies = findQueueFamilies(physicalDevice_);

    vk::CommandPoolCreateInfo poolInfo{
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = queueFamilyIndicies.graphicsFamily.value(),
    };

    commandPool_ = device_.createCommandPool(poolInfo);
}

vk::Format Renderer::findSupportedFormat(
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

bool Renderer::hasStencilComponent(vk::Format format) {
    return format == vk::Format::eD32SfloatS8Uint ||
           format == vk::Format::eD24UnormS8Uint;
}

void Renderer::createTextureImage() {
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load("textures/texture.jpg", &texWidth, &texHeight,
                                &texChannels, STBI_rgb_alpha);
    vk::DeviceSize imageSize = texWidth * texHeight * 4;
    if (!pixels) {
        UB_ERROR("Failed to load texture image.");
    }

    vk::Buffer stagingBuffer;
    vk::DeviceMemory stagingBufferMemory;

    createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc,
                 vk::MemoryPropertyFlagBits::eHostVisible |
                     vk::MemoryPropertyFlagBits::eHostCoherent,
                 stagingBuffer, stagingBufferMemory);
    void* data;
    device_.mapMemory(stagingBufferMemory, 0, imageSize, {}, &data);
    std::memcpy(data, pixels, static_cast<size_t>(imageSize));
    device_.unmapMemory(stagingBufferMemory);

    stbi_image_free(pixels);

    createImage(
        texWidth, texHeight, vk::Format::eR8G8B8A8Srgb,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        vk::MemoryPropertyFlagBits::eDeviceLocal, textureImage_,
        textureImageMemory_);

    transitionImageLayout(textureImage_, vk::Format::eR8G8B8A8Srgb,
                          vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eTransferDstOptimal);
    copyBufferToImage(stagingBuffer, textureImage_,
                      static_cast<uint32_t>(texWidth),
                      static_cast<uint32_t>(texHeight));
    transitionImageLayout(textureImage_, vk::Format::eR8G8B8A8Srgb,
                          vk::ImageLayout::eTransferDstOptimal,
                          vk::ImageLayout::eShaderReadOnlyOptimal);

    device_.destroyBuffer(stagingBuffer);
    device_.freeMemory(stagingBufferMemory);
}

void Renderer::createTextureImageView() {
    textureImageView_ =
        createImageView(textureImage_, vk::Format::eR8G8B8A8Srgb,
                        vk::ImageAspectFlagBits::eColor);
}

void Renderer::createTextureSampler() {
    auto properties = physicalDevice_.getProperties();
    vk::SamplerCreateInfo samplerInfo{
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .mipmapMode = vk::SamplerMipmapMode::eLinear,
        .addressModeU = vk::SamplerAddressMode::eRepeat,
        .addressModeV = vk::SamplerAddressMode::eRepeat,
        .addressModeW = vk::SamplerAddressMode::eRepeat,
        .mipLodBias = 0.0f,
        .anisotropyEnable = vk::True,
        .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
        .compareEnable = vk::False,
        .compareOp = vk::CompareOp::eAlways,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = vk::BorderColor::eIntOpaqueBlack,
        .unnormalizedCoordinates = vk::False,
    };

    textureSampler_ = device_.createSampler(samplerInfo);
}

vk::ImageView Renderer::createImageView(vk::Image image, vk::Format format,
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

void Renderer::createImage(uint32_t width, uint32_t height, vk::Format format,
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

void Renderer::transitionImageLayout(vk::Image image, vk::Format format,
                                     vk::ImageLayout oldLayout,
                                     vk::ImageLayout newLayout) {
    vk::CommandBuffer commandBuffer = beginSingleTimeCommands();

    vk::ImageMemoryBarrier barrier{
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .image = image,
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1},
    };

    vk::PipelineStageFlags sourceStage, destinationStage;

    if (newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;

        if (hasStencilComponent(format)) {
            barrier.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil;
        }
    } else {
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    }

    if (oldLayout == vk::ImageLayout::eUndefined &&
        newLayout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
               newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    } else if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
    } else {
        UB_ERROR("Invalid argument: unsupported layout transition.");
    }

    commandBuffer.pipelineBarrier(sourceStage, destinationStage,  // TODO
                                  {}, 0, nullptr, 0, nullptr, 1, &barrier);

    endSingleTimeCommands(commandBuffer);
}

void Renderer::copyBufferToImage(vk::Buffer buffer, vk::Image image,
                                 uint32_t width, uint32_t height) {
    vk::CommandBuffer commandBuffer = beginSingleTimeCommands();

    vk::BufferImageCopy region{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                             .mipLevel = 0,
                             .baseArrayLayer = 0,
                             .layerCount = 1},
        .imageOffset = {0, 0, 0},
        .imageExtent = {.width = width, .height = height, .depth = 1}};

    commandBuffer.copyBufferToImage(
        buffer, image, vk::ImageLayout::eTransferDstOptimal, 1, &region);

    endSingleTimeCommands(commandBuffer);
}

vk::CommandBuffer Renderer::beginSingleTimeCommands() {
    vk::CommandBufferAllocateInfo allocInfo{
        .commandPool = commandPool_,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    };

    vk::CommandBuffer commandBuffer =
        device_.allocateCommandBuffers(allocInfo)[0];

    vk::CommandBufferBeginInfo beginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};

    commandBuffer.begin(beginInfo);

    return commandBuffer;
}

void Renderer::endSingleTimeCommands(vk::CommandBuffer commandBuffer) {
    commandBuffer.end();

    vk::SubmitInfo submitInfo{.commandBufferCount = 1,
                              .pCommandBuffers = &commandBuffer};

    graphicsQueue_.submit(1, &submitInfo, {});
    graphicsQueue_.waitIdle();
    device_.freeCommandBuffers(commandPool_, 1, &commandBuffer);
}

void Renderer::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                            vk::MemoryPropertyFlags properties,
                            vk::Buffer& buffer,
                            vk::DeviceMemory& bufferMemory) {
    vk::BufferCreateInfo bufferInfo{
        .size = size,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive,
    };
    buffer = device_.createBuffer(bufferInfo);

    auto memRequirements = device_.getBufferMemoryRequirements(buffer);

    vk::MemoryAllocateInfo allocInfo{
        .allocationSize = memRequirements.size,
        .memoryTypeIndex =
            findMemoryType(memRequirements.memoryTypeBits, properties),
    };

    bufferMemory = device_.allocateMemory(allocInfo);
    device_.bindBufferMemory(buffer, bufferMemory, 0);
}

void Renderer::copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer,
                          vk::DeviceSize size) {
    vk::CommandBuffer commandBuffer = beginSingleTimeCommands();

    vk::BufferCopy copyRegion{.size = size};
    commandBuffer.copyBuffer(srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer);
}

void Renderer::createVertexBuffer() {
    vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    vk::Buffer stagingBuffer;
    vk::DeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                 vk::MemoryPropertyFlagBits::eHostVisible |
                     vk::MemoryPropertyFlagBits::eHostCoherent,
                 stagingBuffer, stagingBufferMemory);

    void* data;
    device_.mapMemory(stagingBufferMemory, 0, bufferSize, vk::MemoryMapFlags{},
                      &data);
    std::memcpy(data, vertices.data(), (size_t)bufferSize);
    device_.unmapMemory(stagingBufferMemory);

    createBuffer(bufferSize,
                 vk::BufferUsageFlagBits::eVertexBuffer |
                     vk::BufferUsageFlagBits::eTransferDst,
                 vk::MemoryPropertyFlagBits::eDeviceLocal, vertexBuffer_,
                 vertexBufferMemory_);
    copyBuffer(stagingBuffer, vertexBuffer_, bufferSize);

    device_.destroyBuffer(stagingBuffer);
    device_.freeMemory(stagingBufferMemory);
}

// TODO: indices and vertices should be stored in the same buffer
void Renderer::createIndexBuffer() {
    vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    vk::Buffer stagingBuffer;
    vk::DeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                 vk::MemoryPropertyFlagBits::eHostVisible |
                     vk::MemoryPropertyFlagBits::eHostCoherent,
                 stagingBuffer, stagingBufferMemory);

    void* data;
    device_.mapMemory(stagingBufferMemory, 0, bufferSize, vk::MemoryMapFlags{},
                      &data);
    std::memcpy(data, indices.data(), (size_t)bufferSize);
    device_.unmapMemory(stagingBufferMemory);

    createBuffer(bufferSize,
                 vk::BufferUsageFlagBits::eIndexBuffer |
                     vk::BufferUsageFlagBits::eTransferDst,
                 vk::MemoryPropertyFlagBits::eDeviceLocal, indexBuffer_,
                 indexBufferMemory_);
    copyBuffer(stagingBuffer, indexBuffer_, bufferSize);

    device_.destroyBuffer(stagingBuffer);
    device_.freeMemory(stagingBufferMemory);
}

void Renderer::createUniformBuffers() {
    vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
    uniformBuffers_.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory_.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped_.resize(MAX_FRAMES_IN_FLIGHT);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
                     vk::MemoryPropertyFlagBits::eHostVisible |
                         vk::MemoryPropertyFlagBits::eHostCoherent,
                     uniformBuffers_[i], uniformBuffersMemory_[i]);
        device_.mapMemory(uniformBuffersMemory_[i], 0, bufferSize,
                          vk::MemoryMapFlags{}, &uniformBuffersMapped_[i]);
    }
}

uint32_t Renderer::findMemoryType(uint32_t typeFilter,
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

void Renderer::createCommandBuffers() {
    vk::CommandBufferAllocateInfo allocInfo{
        .commandPool = commandPool_,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = MAX_FRAMES_IN_FLIGHT};

    commandBuffers_ = device_.allocateCommandBuffers(allocInfo);
}

void Renderer::recordCommandBuffer(vk::CommandBuffer commandBuffer) {
    vk::CommandBufferBeginInfo beginInfo{};
    commandBuffer.begin(beginInfo);

    vk::ImageMemoryBarrier imageMemoryBarrier{
        .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
        .oldLayout = vk::ImageLayout::eUndefined,
        .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .image = viewport_.getCurrentImage(),
        .subresourceRange = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                  vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                  {}, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

    vk::RenderingAttachmentInfo colorAttachmentInfo{
        .imageView = viewport_.getCurrentImageView(),
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = {{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}}}
    };

    vk::RenderingAttachmentInfo depthAttachmentInfo {
        .imageView = viewport_.getDepthImageView(),
        .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = {.depthStencil = {1.0f, 0}}
    };

    vk::RenderingInfo renderInfo{
        .renderArea =
            {
                .offset = {0, 0},
                .extent = viewport_.getExtent(),
            },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentInfo,
        .pDepthAttachment = &depthAttachmentInfo,
        // .pStencilAttachment = &depthAttachmentInfo
    };

    commandBuffer.beginRendering(renderInfo, dldy_);
    {
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                   graphicsPipeline_);

        vk::Buffer vertexBuffers[] = {vertexBuffer_};
        vk::DeviceSize offsets[] = {0};
        commandBuffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);
        commandBuffer.bindIndexBuffer(indexBuffer_, 0, vk::IndexType::eUint16);

        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, pipelineLayout_, 0, 1,
            &descriptorSets_[viewport_.getCurrentFrame()], 0, nullptr);

        vk::Viewport viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(viewport_.getExtent().width),
            .height = static_cast<float>(viewport_.getExtent().height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };

        commandBuffer.setViewport(0, 1, &viewport);

        vk::Rect2D scissor{.offset = {0, 0}, .extent = viewport_.getExtent()};

        commandBuffer.setScissor(0, 1, &scissor);

        commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0,
                                  0, 0);
    }
    commandBuffer.endRendering(dldy_);

    vk::ImageMemoryBarrier imageMemoryBarrier2{
        .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
        .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .newLayout = vk::ImageLayout::ePresentSrcKHR,
        .image = viewport_.getCurrentImage(),
        .subresourceRange = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                  vk::PipelineStageFlagBits::eBottomOfPipe,
                                  {}, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier2);


    commandBuffer.end();
}

void Renderer::drawFrame() {
    if (!viewport_.beginFrame()) {
        // Swapchain has been recreated or an error has occurred 
        return;
    }

    const auto currentFrame = viewport_.getCurrentFrame();
    commandBuffers_[currentFrame].reset();
    recordCommandBuffer(commandBuffers_[currentFrame]);

    updateUniformBuffer(currentFrame);

    vk::Semaphore waitSemaphores[] { viewport_.getImageAvailableSemaphore() };

    vk::PipelineStageFlags waitStages[] = {
        vk::PipelineStageFlagBits::eColorAttachmentOutput};

    vk::Semaphore signalSemaphores[] = {
        viewport_.getRenderFinishedSemaphore()};

    vk::SubmitInfo submitInfo{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = waitSemaphores,
        .pWaitDstStageMask = waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffers_[currentFrame],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = signalSemaphores};

    auto submitResult =
        graphicsQueue_.submit(1, &submitInfo, viewport_.getCurrentFrameFence());

    viewport_.endFrame();
}

// TODO: use push constants
void Renderer::updateUniformBuffer(uint32_t currentImage) {
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(
                     currentTime - startTime)
                     .count();

    UniformBufferObject ubo{
        .model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f),
                             glm::vec3(0.0f, 0.0f, 1.0f)),
        .view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f),
                            glm::vec3(0.0f, 0.0f, 0.0f),
                            glm::vec3(0.0f, 0.0f, 1.0f)),
        .proj = glm::perspective(
            glm::radians(45.0f),
            viewport_.getExtent().width / (float)viewport_.getExtent().height, 0.1f,
            10.0f)};
    ubo.proj[1][1] *= -1;

    memcpy(uniformBuffersMapped_[currentImage], &ubo, sizeof(ubo));
}

void Renderer::createDescriptorPool() {
    std::array<vk::DescriptorPoolSize, 2> poolSizes;
    poolSizes[0] = {.type = vk::DescriptorType::eUniformBuffer,
                    .descriptorCount = MAX_FRAMES_IN_FLIGHT};
    poolSizes[1] = {.type = vk::DescriptorType::eCombinedImageSampler,
                    .descriptorCount = MAX_FRAMES_IN_FLIGHT};

    vk::DescriptorPoolCreateInfo poolInfo{
        .maxSets = MAX_FRAMES_IN_FLIGHT,
        .poolSizeCount = poolSizes.size(),
        .pPoolSizes = poolSizes.data(),
    };

    descriptorPool_ = device_.createDescriptorPool(poolInfo);
}

void Renderer::createDescriptorSets() {
    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT,
                                                 descriptorSetLayout_);

    vk::DescriptorSetAllocateInfo allocInfo{
        .descriptorPool = descriptorPool_,
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts = layouts.data()};

    descriptorSets_ = device_.allocateDescriptorSets(allocInfo);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vk::DescriptorBufferInfo bufferInfo{
            .buffer = uniformBuffers_[i],
            .offset = 0,
            .range = sizeof(UniformBufferObject)};

        vk::DescriptorImageInfo imageInfo{
            .sampler = textureSampler_,
            .imageView = textureImageView_,
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        };

        std::array<vk::WriteDescriptorSet, 2> descriptorWrites;
        descriptorWrites[0] = {
            .dstSet = descriptorSets_[i],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .pBufferInfo = &bufferInfo,
        };

        descriptorWrites[1] = {
            .dstSet = descriptorSets_[i],
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .pImageInfo = &imageInfo};
        device_.updateDescriptorSets(descriptorWrites.size(),
                                     descriptorWrites.data(), 0, nullptr);
    }
}

std::vector<char> Renderer::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}

vk::ShaderModule Renderer::createShaderModule(const std::vector<char>& code) {
    vk::ShaderModuleCreateInfo createInfo{
        .codeSize = code.size(),
        .pCode = reinterpret_cast<const uint32_t*>(code.data())};

    return device_.createShaderModule(createInfo, nullptr);
}
const std::vector<const char*> Renderer::validationLayers_ = {
    "VK_LAYER_KHRONOS_validation"};

const std::vector<const char*> Renderer::deviceExtensions_ = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    VK_KHR_MAINTENANCE2_EXTENSION_NAME,
    VK_KHR_MULTIVIEW_EXTENSION_NAME,
    VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
    VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,

};
