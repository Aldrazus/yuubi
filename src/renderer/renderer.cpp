#include <cstring>
#include <limits>

#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

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
    createSwapChain();
    createImageViews();
    createRenderPass();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createCommandPool();
    createDepthResources();
    createFramebuffers();
    createTextureImage();
    createTextureImageView();
    createTextureSampler();
    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();
    createSyncObjects();
}

Renderer::~Renderer() {
    device_.waitIdle();

    cleanupSwapChain();

    device_.destroySampler(textureSampler_);
    device_.destroyImageView(textureImageView_, nullptr);
    device_.destroyImage(textureImage_);
    device_.freeMemory(textureImageMemory_);

    for (size_t i = 0; i < maxFramesInFlight_; i++) {
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
    device_.destroyRenderPass(renderPass_);

    for (uint32_t i = 0; i < maxFramesInFlight_; i++) {
        device_.destroySemaphore(imageAvailableSemaphores_[i]);
        device_.destroySemaphore(renderFinishedSemaphores_[i]);
        device_.destroyFence(inFlightFences_[i]);
    }

    device_.destroyCommandPool(commandPool_);
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

    vk::DeviceCreateInfo createInfo{
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

void Renderer::createSwapChain() {
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

    QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(),
                                     indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = vk::SharingMode::eExclusive;
        createInfo.queueFamilyIndexCount = 0;      // Optional
        createInfo.pQueueFamilyIndices = nullptr;  // Optional
    }

    swapChain_ = device_.createSwapchainKHR(createInfo);
    swapChainImages_ = device_.getSwapchainImagesKHR(swapChain_);
    swapChainImageFormat_ = surfaceFormat.format;
    swapChainExtent_ = swapExtent;
}

void Renderer::cleanupSwapChain() {
    device_.destroyImageView(depthImageView_);
    device_.destroyImage(depthImage_);
    device_.freeMemory(depthImageMemory_);
    for (uint32_t i = 0; i < swapChainFramebuffers_.size(); i++) {
        device_.destroyFramebuffer(swapChainFramebuffers_[i]);
    }
    swapChainFramebuffers_.clear();

    for (uint32_t i = 0; i < swapChainImageViews_.size(); i++) {
        device_.destroyImageView(swapChainImageViews_[i]);
    }
    swapChainImageViews_.clear();

    device_.destroySwapchainKHR(swapChain_);
}

void Renderer::recreateSwapChain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window_.getWindow(), &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window_.getWindow(), &width, &height);
        glfwWaitEvents();
    }
    device_.waitIdle();

    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createDepthResources();
    createFramebuffers();
}

void Renderer::createImageViews() {
    for (const auto& image : swapChainImages_) {
        swapChainImageViews_.push_back(createImageView(
            image, swapChainImageFormat_, vk::ImageAspectFlagBits::eColor));
    }
}

void Renderer::createRenderPass() {
    vk::AttachmentDescription colorAttachment{
        .format = swapChainImageFormat_,
        .samples = vk::SampleCountFlagBits::e1,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
        .initialLayout = vk::ImageLayout::eUndefined,
        .finalLayout = vk::ImageLayout::ePresentSrcKHR};

    vk::AttachmentReference colorAttachmentRef{
        .attachment = 0,
        .layout = vk::ImageLayout::eColorAttachmentOptimal,
    };

    vk::AttachmentDescription depthAttachment{
        .format = findDepthFormat(),
        .samples = vk::SampleCountFlagBits::e1,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eDontCare,
        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
        .initialLayout = vk::ImageLayout::eUndefined,
        .finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal};

    vk::AttachmentReference depthAttachmentRef{
        .attachment = 1,
        .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal};

    vk::SubpassDescription subpass{
        .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef,
        .pDepthStencilAttachment = &depthAttachmentRef};

    vk::SubpassDependency dependency{
        .srcSubpass = vk::SubpassExternal,
        .dstSubpass = 0,
        .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                        vk::PipelineStageFlagBits::eEarlyFragmentTests,
        .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                        vk::PipelineStageFlagBits::eEarlyFragmentTests,
        .srcAccessMask = vk::AccessFlagBits::eNone,
        .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite |
                         vk::AccessFlagBits::eDepthStencilAttachmentWrite};

    std::array<vk::AttachmentDescription, 2> attachments = {colorAttachment,
                                                            depthAttachment};
    vk::RenderPassCreateInfo renderPassInfo{
        .attachmentCount = attachments.size(),
        .pAttachments = attachments.data(),
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency};

    renderPass_ = device_.createRenderPass(renderPassInfo);
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

    vk::GraphicsPipelineCreateInfo pipelineInfo{
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
        .renderPass = renderPass_,
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

void Renderer::createFramebuffers() {
    for (const vk::ImageView& view : swapChainImageViews_) {
        std::array<vk::ImageView, 2> attachments = {view, depthImageView_};

        vk::FramebufferCreateInfo framebufferInfo{
            .renderPass = renderPass_,
            .attachmentCount = attachments.size(),
            .pAttachments = attachments.data(),
            .width = swapChainExtent_.width,
            .height = swapChainExtent_.height,
            .layers = 1,
        };

        swapChainFramebuffers_.push_back(
            device_.createFramebuffer(framebufferInfo));
    }
}

void Renderer::createCommandPool() {
    QueueFamilyIndices queueFamilyIndicies = findQueueFamilies(physicalDevice_);

    vk::CommandPoolCreateInfo poolInfo{
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = queueFamilyIndicies.graphicsFamily.value(),
    };

    commandPool_ = device_.createCommandPool(poolInfo);
}

void Renderer::createDepthResources() {
    vk::Format depthFormat = findDepthFormat();

    createImage(swapChainExtent_.width, swapChainExtent_.height, depthFormat,
                vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eDepthStencilAttachment,
                vk::MemoryPropertyFlagBits::eDeviceLocal, depthImage_,
                depthImageMemory_);

    depthImageView_ = createImageView(depthImage_, depthFormat,
                                      vk::ImageAspectFlagBits::eDepth);
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

vk::Format Renderer::findDepthFormat() {
    return findSupportedFormat(
        {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint,
         vk::Format::eD24UnormS8Uint},
        vk::ImageTiling::eOptimal,
        vk::FormatFeatureFlagBits::eDepthStencilAttachment);
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
    uniformBuffers_.resize(maxFramesInFlight_);
    uniformBuffersMemory_.resize(maxFramesInFlight_);
    uniformBuffersMapped_.resize(maxFramesInFlight_);

    for (uint32_t i = 0; i < maxFramesInFlight_; i++) {
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
        .commandBufferCount = maxFramesInFlight_};

    commandBuffers_ = device_.allocateCommandBuffers(allocInfo);
}

void Renderer::recordCommandBuffer(vk::CommandBuffer commandBuffer,
                                   uint32_t imageIndex) {
    vk::CommandBufferBeginInfo beginInfo{};
    commandBuffer.begin(beginInfo);

    std::array<vk::ClearValue, 2> clearValues{};
    clearValues[0].color = {{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}}};
    clearValues[1].depthStencil = {1.0f, 0};

    vk::RenderPassBeginInfo renderPassInfo{
        .renderPass = renderPass_,
        .framebuffer = swapChainFramebuffers_[imageIndex],
        .renderArea =
            {
                .offset = {0, 0},
                .extent = swapChainExtent_,
            },
        .clearValueCount = clearValues.size(),
        .pClearValues = clearValues.data()};

    commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
    {
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                   graphicsPipeline_);

        vk::Buffer vertexBuffers[] = {vertexBuffer_};
        vk::DeviceSize offsets[] = {0};
        commandBuffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);
        commandBuffer.bindIndexBuffer(indexBuffer_, 0, vk::IndexType::eUint16);

        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, pipelineLayout_, 0, 1,
            &descriptorSets_[currentFrame_], 0, nullptr);

        vk::Viewport viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(swapChainExtent_.width),
            .height = static_cast<float>(swapChainExtent_.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };

        commandBuffer.setViewport(0, 1, &viewport);

        vk::Rect2D scissor{.offset = {0, 0}, .extent = swapChainExtent_};

        commandBuffer.setScissor(0, 1, &scissor);

        commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0,
                                  0, 0);
    }
    commandBuffer.endRenderPass();

    commandBuffer.end();
}

void Renderer::createSyncObjects() {
    for (uint32_t i = 0; i < maxFramesInFlight_; i++) {
        imageAvailableSemaphores_.push_back(device_.createSemaphore({}));
        renderFinishedSemaphores_.push_back(device_.createSemaphore({}));
        inFlightFences_.push_back(
            device_.createFence({.flags = vk::FenceCreateFlagBits::eSignaled}));
    }
}

void Renderer::drawFrame() {
    auto waitResult =
        device_.waitForFences(1, &inFlightFences_[currentFrame_], vk::True,
                              std::numeric_limits<uint64_t>::max());

    uint32_t imageIndex;
    // TODO: remove exceptions
    try {
        // This line gets immediately gets the index of the next image in the
        // swapchain. This image may not be available for immediate use, so a
        // semaphore is used to synchronize commands that require this image
        auto [result, idx] = device_.acquireNextImageKHR(
            swapChain_, std::numeric_limits<uint64_t>::max(),
            imageAvailableSemaphores_[currentFrame_]);
        imageIndex = idx;
    } catch (const std::exception& e) {
        recreateSwapChain();
        return;
    }

    auto resetFenceResult =
        device_.resetFences(1, &inFlightFences_[currentFrame_]);

    commandBuffers_[currentFrame_].reset();
    recordCommandBuffer(commandBuffers_[currentFrame_], imageIndex);

    updateUniformBuffer(currentFrame_);

    vk::Semaphore waitSemaphores[] = {imageAvailableSemaphores_[currentFrame_]};
    vk::PipelineStageFlags waitStages[] = {
        vk::PipelineStageFlagBits::eColorAttachmentOutput};

    vk::Semaphore signalSemaphores[] = {
        renderFinishedSemaphores_[currentFrame_]};

    vk::SubmitInfo submitInfo{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = waitSemaphores,
        .pWaitDstStageMask = waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffers_[currentFrame_],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = signalSemaphores};

    auto submitResult =
        graphicsQueue_.submit(1, &submitInfo, inFlightFences_[currentFrame_]);

    vk::SwapchainKHR swapchains[] = {swapChain_};

    vk::PresentInfoKHR presentInfo{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = signalSemaphores,
        .swapchainCount = 1,
        .pSwapchains = swapchains,
        .pImageIndices = &imageIndex,
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

    currentFrame_ = (currentFrame_ + 1) % maxFramesInFlight_;
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
            swapChainExtent_.width / (float)swapChainExtent_.height, 0.1f,
            10.0f)};
    ubo.proj[1][1] *= -1;

    memcpy(uniformBuffersMapped_[currentImage], &ubo, sizeof(ubo));
}

void Renderer::createDescriptorPool() {
    std::array<vk::DescriptorPoolSize, 2> poolSizes;
    poolSizes[0] = {.type = vk::DescriptorType::eUniformBuffer,
                    .descriptorCount = maxFramesInFlight_};
    poolSizes[1] = {.type = vk::DescriptorType::eCombinedImageSampler,
                    .descriptorCount = maxFramesInFlight_};

    vk::DescriptorPoolCreateInfo poolInfo{
        .maxSets = maxFramesInFlight_,
        .poolSizeCount = poolSizes.size(),
        .pPoolSizes = poolSizes.data(),
    };

    descriptorPool_ = device_.createDescriptorPool(poolInfo);
}

void Renderer::createDescriptorSets() {
    std::vector<vk::DescriptorSetLayout> layouts(maxFramesInFlight_,
                                                 descriptorSetLayout_);

    vk::DescriptorSetAllocateInfo allocInfo{
        .descriptorPool = descriptorPool_,
        .descriptorSetCount = maxFramesInFlight_,
        .pSetLayouts = layouts.data()};

    descriptorSets_ = device_.allocateDescriptorSets(allocInfo);

    for (size_t i = 0; i < maxFramesInFlight_; i++) {
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
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};
