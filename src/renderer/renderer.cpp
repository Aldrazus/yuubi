#include "renderer/renderer.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <stb_image.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "application.h"
#include "renderer/camera.h"
#include "renderer/passes/depth_pass.h"
#include "renderer/loaded_gltf.h"
#include "renderer/passes/lighting_pass.h"
#include "renderer/render_object.h"
#include "renderer/vma/buffer.h"
#include "renderer/vulkan_usage.h"
#include "renderer/pipeline_builder.h"
#include "renderer/descriptor_layout_builder.h"
#include "renderer/vulkan/util.h"
#include "renderer/push_constants.h"
#include "renderer/gpu_data.h"
#include "pch.h"

namespace yuubi {

Renderer::Renderer(const Window& window) : window_(window) {
    instance_ = Instance{context_};

    VkSurfaceKHR tmp;
    if (glfwCreateWindowSurface(
            static_cast<VkInstance>(*instance_.getInstance()),
            window_.getWindow(), nullptr, &tmp
        ) != VK_SUCCESS) {
        UB_ERROR("Unable to create window surface!");
    }

    surface_ = std::make_shared<vk::raii::SurfaceKHR>(instance_, tmp);
    device_ = std::make_shared<Device>(instance_, *surface_);
    viewport_ = std::make_shared<Viewport>(surface_, device_);
    textureManager_ = TextureManager(device_);
    imguiManager_ = ImguiManager{instance_, *device_, window_, *viewport_};

    materialManager_ = MaterialManager(device_);

    // asset_ = GLTFAsset(*device_, textureManager_, materialManager_,
    // "assets/monkey/monkey.glb");
    asset_ = GLTFAsset(
        *device_, textureManager_, materialManager_, "assets/sponza/Sponza.gltf"
    );

    {
        vk::DeviceSize bufferSize = sizeof(SceneData);
        vk::BufferCreateInfo bufferCreateInfo{
            .size = bufferSize,
            .usage = vk::BufferUsageFlagBits::eStorageBuffer |
                     vk::BufferUsageFlagBits::eTransferDst |
                     vk::BufferUsageFlagBits::eShaderDeviceAddress
        };

        VmaAllocationCreateInfo shaderDataBufferAllocInfo{
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        };

        sceneDataBuffer_ =
            device_->createBuffer(bufferCreateInfo, shaderDataBufferAllocInfo);

        SceneData data{
            .view = {},
            .proj = {},
            .viewproj = {},
            .cameraPosition = {},
            .ambientColor = {},
            .sunlightDirection = {},
            .sunlightColor = {},
            .materials = materialManager_.getBufferAddress(),
        };

        sceneDataBuffer_.upload(*device_, &data, sizeof(data), 0);
    }

    initSkybox();
    initFinalPassResources();

    depthPass_ = DepthPass(device_, viewport_);

    // Create normal attachment.
    createNormalAttachment();

    // Create lighting pass.
    std::vector<vk::DescriptorSetLayout> setLayouts = {
        *textureManager_.getTextureSetLayout()
    };

    std::vector<vk::PushConstantRange> pushConstantRanges = {
        vk::PushConstantRange{
                              .stageFlags = vk::ShaderStageFlagBits::eVertex |
                              vk::ShaderStageFlagBits::eFragment,
                              .offset = 0,
                              .size = sizeof(PushConstants),
                              }
    };

    std::array<vk::Format, 2> formats{
        // TODO: reevaluate normal format, maybe Snorm?
        viewport_->getDrawImageFormat(), normalFormat_
    };

    lightingPass_ = LightingPass(LightingPass::CreateInfo{
        .device = device_,
        .descriptorSetLayouts = setLayouts,
        .pushConstantRanges = pushConstantRanges,
        .colorAttachmentFormats = formats,
        .depthFormat = viewport_->getDepthFormat()
    });
}

Renderer::~Renderer() { device_->getDevice().waitIdle(); }

void Renderer::updateScene(const Camera& camera) {
    drawContext_.opaqueSurfaces.clear();

    asset_.draw(glm::mat4(1.0f), drawContext_);

    SceneData data{
        .view = camera.getViewMatrix(),
        .proj =
            camera.getViewProjectionMatrix(),  // TODO: only push proj matrix
        .viewproj = camera.getViewProjectionMatrix(),
        .cameraPosition = glm::vec4(camera.getPosition(), 1.0),
        .ambientColor = glm::vec4(0.1f),
        .sunlightDirection = glm::vec4(0, 1, 0.f, 1.0f),
        .sunlightColor = glm::vec4(1.0f),
        .materials = materialManager_.getBufferAddress(),
    };
    sceneDataBuffer_.upload(*device_, &data, sizeof(data), 0);
}

void Renderer::draw(const Camera& camera, AppState state) {
    updateScene(camera);

    // TODO: move to ImguiManager somehow
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Average FPS");
    ImGui::Text("Average FPS: %d", (uint32_t)state.averageFPS);
    ImGui::End();

    ImGui::Render();

    // TODO: fix formatted lambda args causing misaligned indents
    viewport_->doFrame([this, &camera](
                           const Frame& frame, const SwapchainImage& image,
                           const Image& drawImage,
                           const vk::raii::ImageView& drawImageView
                       ) {
        vk::CommandBufferBeginInfo beginInfo{};
        frame.commandBuffer.begin(beginInfo);

        // Transition swapchain image layout to COLOR_ATTACHMENT_OPTIMAL
        // before rendering
        transitionImage(
            frame.commandBuffer, image.image, vk::ImageLayout::eUndefined,
            vk::ImageLayout::eColorAttachmentOptimal
        );

        depthPass_.render(frame.commandBuffer, drawContext_, sceneDataBuffer_);

        std::vector<vk::DescriptorSet> descriptorSets{
            textureManager_.getTextureSet()
        };

        lightingPass_.render(LightingPass::RenderInfo{
            .commandBuffer = frame.commandBuffer,
            .context = drawContext_,
            .viewportExtent = viewport_->getExtent(),
            .descriptorSets = descriptorSets,
            .sceneDataBuffer = sceneDataBuffer_,
            .color =
                RenderAttachment{
                              .image = drawImage.getImage(), .imageView = drawImageView
                },
            .normal =
                RenderAttachment{
                              .image = normalImage_.getImage(),
                              .imageView = normalImageView_
                },
            .depth =
                RenderAttachment{
                              viewport_->getDepthImage().getImage(),
                              viewport_->getDepthImageView()
                }
        });

        // Transition swapchain image layout to PRESENT_SRC before
        // presenting
        transitionImage(
            frame.commandBuffer, image.image,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::ePresentSrcKHR
        );

        frame.commandBuffer.end();

        vk::Semaphore waitSemaphores[]{*frame.imageAvailable};
        vk::PipelineStageFlags waitStages[]{
            vk::PipelineStageFlagBits::eColorAttachmentOutput
        };

        vk::Semaphore signalSemaphores[]{*frame.renderFinished};

        vk::SubmitInfo submitInfo{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = waitSemaphores,
            .pWaitDstStageMask = waitStages,
            .commandBufferCount = 1,
            .pCommandBuffers = &*frame.commandBuffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = signalSemaphores
        };

        device_->getQueue().queue.submit({submitInfo}, *frame.inFlight);
    });
}

void Renderer::initSkybox() {
    // Create descriptor set/layout.
    DescriptorLayoutBuilder layoutBuilder(device_);

    skyboxDescriptorSetLayout_ =
        layoutBuilder
            .addBinding(vk::DescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eFragment
            })
            .build(
                vk::DescriptorSetLayoutBindingFlagsCreateInfo{
                    .bindingCount = 0, .pBindingFlags = nullptr
                },
                vk::DescriptorSetLayoutCreateFlags{}
            );

    std::vector<vk::DescriptorPoolSize> poolSizes{
        vk::DescriptorPoolSize{
                               .type = vk::DescriptorType::eCombinedImageSampler,
                               .descriptorCount = 1}
    };

    vk::DescriptorPoolCreateInfo poolInfo{
        .maxSets = 1,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data(),
    };

    skyboxDescriptorPool_ = device_->getDevice().createDescriptorPool(poolInfo);

    vk::DescriptorSetAllocateInfo allocInfo{
        .descriptorPool = *skyboxDescriptorPool_,
        .descriptorSetCount = 1,
        .pSetLayouts = &*skyboxDescriptorSetLayout_
    };

    vk::raii::DescriptorSets sets(device_->getDevice(), allocInfo);
    skyboxDescriptorSet_ = vk::raii::DescriptorSet(std::move(sets[0]));

    // Create pipeline.
    auto vertShader = loadShader("shaders/skybox.vert.spv", *device_);
    auto fragShader = loadShader("shaders/skybox.frag.spv", *device_);

    std::vector<vk::PushConstantRange> pushConstantRanges = {
        vk::PushConstantRange{
                              .stageFlags = vk::ShaderStageFlagBits::eVertex,
                              .offset = 0,
                              .size = sizeof(glm::mat4),
                              }
    };

    std::vector<vk::DescriptorSetLayout> descriptorSetLayouts = {
        *skyboxDescriptorSetLayout_
    };

    skyboxPipelineLayout_ = createPipelineLayout(
        *device_, descriptorSetLayouts, pushConstantRanges
    );
    PipelineBuilder builder(skyboxPipelineLayout_);

    std::array<vk::Format, 2> formats = {
        viewport_->getSwapChainImageFormat(), viewport_->getDrawImageFormat()
    };

    skyboxPipeline_ =
        builder.setShaders(vertShader, fragShader)
            .setInputTopology(vk::PrimitiveTopology::eTriangleList)
            .setPolygonMode(vk::PolygonMode::eFill)
            // TODO: simplify by just culling the front faces
            .setCullMode(
                vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise
            )
            .setMultisamplingNone()
            .disableBlending()
            .enableDepthTest(true, vk::CompareOp::eGreaterOrEqual)
            .setColorAttachmentFormats(formats)
            .setDepthFormat(viewport_->getDepthFormat())
            .build(*device_);

    const uint32_t numIndices = 6 * 2 * 3;
    std::array<uint32_t, numIndices> indices = {
        0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17,
        18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35
    };

    vk::DeviceSize bufferSize = sizeof(indices[0]) * numIndices;
    skyboxIndexBuffer_ = Buffer(
        &device_->allocator(),
        vk::BufferCreateInfo{
            .size = bufferSize,
            .usage = vk::BufferUsageFlagBits::eIndexBuffer |
                     vk::BufferUsageFlagBits::eTransferDst
        },
        VmaAllocationCreateInfo{.usage = VMA_MEMORY_USAGE_GPU_ONLY}
    );

    skyboxIndexBuffer_.upload(*device_, indices.data(), bufferSize, 0);

    // Load skybox images.

    // Per Vulkan 1.3 Section 12.5:
    // For cube and cube array image views, the layers of the image view
    // starting at baseArrayLayer correspond to faces in the order +X, -X, +Y,
    // -Y, +Z, -Z.
    const std::array<std::string, 6> filePaths = {
        "assets/skybox/right.jpg", "assets/skybox/left.jpg",
        "assets/skybox/top.jpg",   "assets/skybox/bottom.jpg",
        "assets/skybox/front.jpg", "assets/skybox/back.jpg",
    };

    std::array<stbi_uc*, 6> sidePixels;
    int width, height, numChannels;

    for (const auto& [i, path] : filePaths | std::views::enumerate) {
        UB_INFO("Loading side {}: {}", i, path);
        sidePixels[i] =
            stbi_load(path.c_str(), &width, &height, &numChannels, 4);
        if (sidePixels[i] == nullptr) {
            UB_ERROR("sizePixels is nullptr");
        }
        numChannels = 4;
    }

    vk::DeviceSize imageSize = width * height * 4 * sizeof(stbi_uc) * 6;

    vk::BufferCreateInfo stagingBufferCreateInfo{
        .size = imageSize,
        .usage = vk::BufferUsageFlagBits::eTransferSrc,
    };

    VmaAllocationCreateInfo stagingBufferAllocCreateInfo{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    Buffer stagingBuffer = device_->createBuffer(
        stagingBufferCreateInfo, stagingBufferAllocCreateInfo
    );

    for (auto&& [i, ptr] : sidePixels | std::views::enumerate) {
        const size_t size = width * height * numChannels * sizeof(stbi_uc);
        auto pixels = std::span{ptr, size};
        std::ranges::copy(
            pixels,
            static_cast<stbi_uc*>(stagingBuffer.getMappedMemory()) + size * i
        );
        stbi_image_free(ptr);
    }

    skyboxImage_ = Image(
        &device_->allocator(),
        ImageCreateInfo{
            .width = static_cast<uint32_t>(width),
            .height = static_cast<uint32_t>(height),
            .format = vk::Format::eR8G8B8A8Srgb,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = vk::ImageUsageFlagBits::eSampled |
                     vk::ImageUsageFlagBits::eTransferSrc |
                     vk::ImageUsageFlagBits::eTransferDst,
            .properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
            .mipLevels = 1,
            .arrayLayers = 6
        }
    );

    device_->submitImmediateCommands(
        [&stagingBuffer, this, width,
         height](const vk::raii::CommandBuffer& commandBuffer) {
            transitionImage(
                commandBuffer, *skyboxImage_.getImage(),
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eTransferDstOptimal
            );

            vk::BufferImageCopy copyRegion{
                .bufferOffset = 0,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource =
                    vk::ImageSubresourceLayers{
                                               .aspectMask = vk::ImageAspectFlagBits::eColor,
                                               .mipLevel = 0,
                                               .baseArrayLayer = 0,
                                               .layerCount = 6
                    },
                .imageOffset = {0, 0, 0},
                .imageExtent =
                    vk::Extent3D{
                                               .width = static_cast<uint32_t>(width),
                                               .height = static_cast<uint32_t>(height),
                                               .depth = 1
                    }
            };

            commandBuffer.copyBufferToImage(
                *stagingBuffer.getBuffer(), *skyboxImage_.getImage(),
                vk::ImageLayout::eTransferDstOptimal, {copyRegion}
            );

            transitionImage(
                commandBuffer, *skyboxImage_.getImage(),
                vk::ImageLayout::eTransferDstOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal
            );
        }
    );

    // Create image view.
    skyboxImageView_ = device_->createImageView(
        *skyboxImage_.getImage(), vk::Format::eR8G8B8A8Srgb,
        vk::ImageAspectFlagBits::eColor, 1, vk::ImageViewType::eCube
    );

    // Create sampler.
    skyboxSampler_ = device_->getDevice().createSampler(vk::SamplerCreateInfo{
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .mipmapMode = vk::SamplerMipmapMode::eLinear,
        .addressModeU = vk::SamplerAddressMode::eRepeat,
        .addressModeV = vk::SamplerAddressMode::eRepeat,
        .addressModeW = vk::SamplerAddressMode::eRepeat,
        .mipLodBias = 0.0f,
        .anisotropyEnable = vk::True,
        .maxAnisotropy = device_->getPhysicalDevice()
                             .getProperties()
                             .limits.maxSamplerAnisotropy,
        .compareEnable = vk::False,
        .compareOp = vk::CompareOp::eAlways,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = vk::BorderColor::eIntOpaqueBlack,
        .unnormalizedCoordinates = vk::False,
    });

    // Update descriptor set.
    vk::DescriptorImageInfo descImageInfo{
        .sampler = *skyboxSampler_,
        .imageView = *skyboxImageView_,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };

    device_->getDevice().updateDescriptorSets(
        {
            vk::WriteDescriptorSet{
                                   .dstSet = *skyboxDescriptorSet_,
                                   .dstBinding = 0,
                                   .dstArrayElement = 0,
                                   .descriptorCount = 1,
                                   .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                                   .pImageInfo = &descImageInfo}
    },
        {}
    );
}

void Renderer::initFinalPassResources() {
    // Create descriptor set/layout.
    DescriptorLayoutBuilder layoutBuilder(device_);

    finalDescriptorSetLayout_ =
        layoutBuilder
            .addBinding(vk::DescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = vk::DescriptorType::eInputAttachment,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eFragment
            })
            .build(
                vk::DescriptorSetLayoutBindingFlagsCreateInfo{
                    .bindingCount = 0, .pBindingFlags = nullptr
                },
                vk::DescriptorSetLayoutCreateFlags{}
            );

    std::vector<vk::DescriptorPoolSize> poolSizes{
        vk::DescriptorPoolSize{
                               .type = vk::DescriptorType::eInputAttachment, .descriptorCount = 1}
    };

    vk::DescriptorPoolCreateInfo poolInfo{
        .maxSets = 1,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data(),
    };

    finalDescriptorPool_ = device_->getDevice().createDescriptorPool(poolInfo);

    vk::DescriptorSetAllocateInfo allocInfo{
        .descriptorPool = *finalDescriptorPool_,
        .descriptorSetCount = 1,
        .pSetLayouts = &*finalDescriptorSetLayout_
    };

    vk::raii::DescriptorSets sets(device_->getDevice(), allocInfo);
    finalDescriptorSet_ = vk::raii::DescriptorSet(std::move(sets[0]));

    // Create pipeline.
    auto vertShader = loadShader("shaders/screen_quad.vert.spv", *device_);
    auto fragShader = loadShader("shaders/screen_quad.frag.spv", *device_);

    std::vector<vk::DescriptorSetLayout> descriptorSetLayouts = {
        *finalDescriptorSetLayout_
    };

    finalPipelineLayout_ =
        createPipelineLayout(*device_, descriptorSetLayouts, {});
    PipelineBuilder builder(finalPipelineLayout_);

    std::array<vk::Format, 2> formats = {
        viewport_->getSwapChainImageFormat(), viewport_->getDrawImageFormat()
    };

    finalPipeline_ =
        builder.setShaders(vertShader, fragShader)
            .setInputTopology(vk::PrimitiveTopology::eTriangleList)
            .setPolygonMode(vk::PolygonMode::eFill)
            .setCullMode(
                vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise
            )
            .setMultisamplingNone()
            .disableBlending()
            .enableDepthTest(true, vk::CompareOp::eGreaterOrEqual)
            .setColorAttachmentFormats(formats)
            .setDepthFormat(viewport_->getDepthFormat())
            .build(*device_);

    const uint32_t numIndices = 6;
    std::array<uint32_t, numIndices> indices = {0, 1, 2, 1, 3, 2};

    vk::DeviceSize bufferSize = sizeof(indices[0]) * numIndices;
    finalIndexBuffer_ = Buffer(
        &device_->allocator(),
        vk::BufferCreateInfo{
            .size = bufferSize,
            .usage = vk::BufferUsageFlagBits::eIndexBuffer |
                     vk::BufferUsageFlagBits::eTransferDst
        },
        VmaAllocationCreateInfo{.usage = VMA_MEMORY_USAGE_GPU_ONLY}
    );

    finalIndexBuffer_.upload(*device_, indices.data(), bufferSize, 0);

    // Update descriptor set.
    vk::DescriptorImageInfo descImageInfo{
        .sampler = nullptr,
        .imageView = *viewport_->getDrawImageView(),
        .imageLayout = vk::ImageLayout::eRenderingLocalReadKHR,
    };

    device_->getDevice().updateDescriptorSets(
        {
            vk::WriteDescriptorSet{
                                   .dstSet = *finalDescriptorSet_,
                                   .dstBinding = 0,
                                   .dstArrayElement = 0,
                                   .descriptorCount = 1,
                                   .descriptorType = vk::DescriptorType::eInputAttachment,
                                   .pImageInfo = &descImageInfo}
    },
        {}
    );
}

void Renderer::createNormalAttachment() {
    normalImage_ = Image(
        &device_->allocator(),
        ImageCreateInfo{
            .width = viewport_->getExtent().width,
            .height = viewport_->getExtent().height,
            .format = normalFormat_,
            .tiling = vk::ImageTiling::eOptimal,
            // TODO: check if this should be a sampled image or an input
            // attachment.
            .usage = vk::ImageUsageFlagBits::eColorAttachment |
                     vk::ImageUsageFlagBits::eSampled,
            .properties = vk::MemoryPropertyFlagBits::eDeviceLocal
        }
    );

    normalImageView_ = device_->createImageView(
        *normalImage_.getImage(), normalFormat_, vk::ImageAspectFlagBits::eColor
    );

    device_->submitImmediateCommands(
        [this](const vk::raii::CommandBuffer& commandBuffer) {
            vk::ImageMemoryBarrier2 imageMemoryBarrier{
                .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
                .srcAccessMask = vk::AccessFlagBits2::eNone,
                .dstStageMask =
                    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                .oldLayout = vk::ImageLayout::eUndefined,
                .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .image = *normalImage_.getImage(),
                .subresourceRange{
                                  .aspectMask = vk::ImageAspectFlagBits::eColor,
                                  .baseMipLevel = 0,
                                  .levelCount = vk::RemainingMipLevels,
                                  .baseArrayLayer = 0,
                                  .layerCount = vk::RemainingArrayLayers},
            };

            vk::DependencyInfo dependencyInfo{
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = &imageMemoryBarrier
            };
            commandBuffer.pipelineBarrier2(dependencyInfo);
        }
    );
}

}
