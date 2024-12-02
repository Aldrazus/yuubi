#include "renderer/renderer.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <stb_image.h>
#include <random>

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
        if (glfwCreateWindowSurface(*instance_.getInstance(), window_.getWindow(), nullptr, &tmp) != VK_SUCCESS) {
            UB_ERROR("Unable to create window surface!");
        }

        surface_ = std::make_shared<vk::raii::SurfaceKHR>(instance_.getInstance(), tmp);
        device_ = std::make_shared<Device>(instance_.getInstance(), *surface_);
        viewport_ = std::make_shared<Viewport>(surface_, device_);
        textureManager_ = TextureManager(device_);
        imguiManager_ = ImguiManager{instance_, *device_, window_, *viewport_};

        materialManager_ = MaterialManager(device_);

        asset_ = GLTFAsset(*device_, textureManager_, materialManager_, "assets/sponza/Sponza.gltf");

        {
            constexpr vk::DeviceSize bufferSize = sizeof(SceneData);
            constexpr vk::BufferCreateInfo bufferCreateInfo{
                    .size = bufferSize,
                    .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst |
                             vk::BufferUsageFlagBits::eShaderDeviceAddress
            };

            constexpr VmaAllocationCreateInfo shaderDataBufferAllocInfo{
                    .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            };

            sceneDataBuffer_ = device_->createBuffer(bufferCreateInfo, shaderDataBufferAllocInfo);

            const SceneData data{
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

        initCubemapPassResources();
        initSkybox();
        initCompositePassResources();

        {
            std::vector setLayouts{*textureManager_.getTextureSetLayout()};
            depthPass_ = DepthPass(device_, viewport_, setLayouts);
        }

        // Create normal attachment.
        createNormalAttachment();

        // Create lighting pass.
        std::vector setLayouts = {*textureManager_.getTextureSetLayout()};

        std::vector pushConstantRanges = {
                vk::PushConstantRange{
                                      .stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                                      .offset = 0,
                                      .size = sizeof(PushConstants),
                                      }
        };

        std::array formats{
                // TODO: reevaluate normal format, maybe Snorm?
                viewport_->getDrawImageFormat(), normalFormat_
        };

        lightingPass_ = LightingPass(
                LightingPass::CreateInfo{
                        .device = device_,
                        .descriptorSetLayouts = setLayouts,
                        .pushConstantRanges = pushConstantRanges,
                        .colorAttachmentFormats = formats,
                        .depthFormat = viewport_->getDepthFormat()
                }
        );


        initAOPassResources();
    }

    Renderer::~Renderer() { device_->getDevice().waitIdle(); }

    void Renderer::updateScene(const Camera& camera) {
        drawContext_.opaqueSurfaces.clear();

        asset_.draw(glm::mat4(1.0f), drawContext_);

        const SceneData data{
                .view = camera.getViewMatrix(),
                .proj = camera.getViewProjectionMatrix(), // TODO: only push proj matrix
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
        ImGui::Text("Average FPS: %d", static_cast<uint32_t>(state.averageFPS));
        ImGui::End();

        ImGui::Render();

        // TODO: fix formatted lambda args causing misaligned indents
        viewport_->doFrame([this, &camera](
                                   const Frame& frame, const SwapchainImage& image, const Image& drawImage,
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

            // Depth pre-pass
            depthPass_.render(frame.commandBuffer, drawContext_, sceneDataBuffer_, textureManager_.getTextureSet());

            std::vector<vk::DescriptorSet> descriptorSets{textureManager_.getTextureSet()};

            // Transition draw image.
            {
                vk::ImageMemoryBarrier2 imageMemoryBarrier{
                        // PERF: we really toppin da pipe with this one
                        .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
                        .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                        .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                        .oldLayout = vk::ImageLayout::eUndefined,
                        .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
                        .image = *drawImage.getImage(),
                        .subresourceRange{
                                          .aspectMask = vk::ImageAspectFlagBits::eColor,
                                          .baseMipLevel = 0,
                                          .levelCount = vk::RemainingMipLevels,
                                          .baseArrayLayer = 0,
                                          .layerCount = vk::RemainingArrayLayers
                        },
                };

                vk::DependencyInfo dependencyInfo{
                        .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &imageMemoryBarrier
                };
                frame.commandBuffer.pipelineBarrier2(dependencyInfo);
            }

            // Transition normal image.
            {
                vk::ImageMemoryBarrier2 imageMemoryBarrier{
                        // PERF: we really toppin da pipe with this one
                        .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
                        .srcAccessMask = vk::AccessFlagBits2::eNone,
                        .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                        .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                        .oldLayout = vk::ImageLayout::eUndefined,
                        .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
                        .image = *normalImage_.getImage(),
                        .subresourceRange{
                                          .aspectMask = vk::ImageAspectFlagBits::eColor,
                                          .baseMipLevel = 0,
                                          .levelCount = vk::RemainingMipLevels,
                                          .baseArrayLayer = 0,
                                          .layerCount = vk::RemainingArrayLayers
                        },
                };

                vk::DependencyInfo dependencyInfo{
                        .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &imageMemoryBarrier
                };
                frame.commandBuffer.pipelineBarrier2(dependencyInfo);
            }

            // Lighting pass.
            lightingPass_.render(
                    LightingPass::RenderInfo{
                            .commandBuffer = frame.commandBuffer,
                            .context = drawContext_,
                            .viewportExtent = viewport_->getExtent(),
                            .descriptorSets = descriptorSets,
                            .sceneDataBuffer = sceneDataBuffer_,
                            .color = RenderAttachment{.image = drawImage.getImage(),     .imageView = drawImageView                                                      },
                            .normal = RenderAttachment{     .image = normalImage_.getImage(),  .imageView = normalImageView_},
                            .depth =
                                    RenderAttachment{
                                                      viewport_->getDepthImage().getImage(), viewport_->getDepthImageView()}
            }
            );

            // Transition cubemap image to COLOR_ATTACHMENT_OPTIMAL
            {
                vk::ImageMemoryBarrier2 imageMemoryBarrier{
                        // PERF: we really toppin da pipe with this one
                        .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
                        .srcAccessMask = vk::AccessFlagBits2::eNone,
                        .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                        .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                        .oldLayout = vk::ImageLayout::eUndefined,
                        .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
                        .image = *cubemapImage_.getImage(),
                        .subresourceRange{
                                          .aspectMask = vk::ImageAspectFlagBits::eColor,
                                          .baseMipLevel = 0,
                                          .levelCount = vk::RemainingMipLevels,
                                          .baseArrayLayer = 0,
                                          .layerCount = vk::RemainingArrayLayers
                        },
                };

                vk::DependencyInfo dependencyInfo{
                        .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &imageMemoryBarrier
                };
                frame.commandBuffer.pipelineBarrier2(dependencyInfo);
            }

            // Equirectangular to Cubemap pass.
            {
                cubemapPass_.render(
                        CubemapPass::RenderInfo{
                                .commandBuffer = frame.commandBuffer,
                                .viewportExtent = vk::Extent2D(512, 512),
                                .color =
                                        RenderAttachment{
                                                         .image = cubemapImage_.getImage(), .imageView = cubemapImageView_
                                        },
                                .descriptorSets = {}
                }
                );
            }

            // Transition cubemap image.
            // Transition draw image.
            {
                vk::ImageMemoryBarrier2 imageMemoryBarrier{
                        .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                        .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
                        .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                        .dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
                        .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
                        .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                        .image = cubemapImage_.getImage(),
                        .subresourceRange{
                                          .aspectMask = vk::ImageAspectFlagBits::eColor,
                                          .baseMipLevel = 0,
                                          .levelCount = vk::RemainingMipLevels,
                                          .baseArrayLayer = 0,
                                          .layerCount = vk::RemainingArrayLayers
                        },
                };

                vk::DependencyInfo dependencyInfo{
                        .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &imageMemoryBarrier
                };
                frame.commandBuffer.pipelineBarrier2(dependencyInfo);
            }

            // Skybox pass.
            {
                std::vector descriptorSets{*skyboxDescriptorSet_};

                const auto viewProjection = camera.getProjectionMatrix() * glm::mat4(glm::mat3(camera.getViewMatrix()));
                skyboxPass_.render(
                        SkyboxPass::RenderInfo{
                                .commandBuffer = frame.commandBuffer,
                                .descriptorSets = {descriptorSets},
                                .color = RenderAttachment{.image = drawImage.getImage(), .imageView = drawImageView},
                                .pushConstants = SkyboxPass::PushConstants{.viewProjection = viewProjection},
                                .viewportExtent = viewport_->getExtent(),
                                .depth =
                                        RenderAttachment{
                                                   viewport_->getDepthImage().getImage(), viewport_->getDepthImageView()
                                        }
                }
                );
            }

            // Transition normal image.
            {
                vk::ImageMemoryBarrier2 normalImageBarrier{
                        .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                        .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                        .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
                        // TODO: is this correct?
                        .dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
                        .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
                        .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                        .image = normalImage_.getImage(),
                        .subresourceRange{
                                          .aspectMask = vk::ImageAspectFlagBits::eColor,
                                          .baseMipLevel = 0,
                                          .levelCount = vk::RemainingMipLevels,
                                          .baseArrayLayer = 0,
                                          .layerCount = vk::RemainingArrayLayers
                        }
                };

                vk::DependencyInfo dependencyInfo{
                        .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &normalImageBarrier
                };
                frame.commandBuffer.pipelineBarrier2(dependencyInfo);
            }

            // Transition depth image.
            {
                vk::ImageMemoryBarrier2 depthImageBarrier{
                        // TODO: fix srcstagemask
                        .srcStageMask = vk::PipelineStageFlagBits2::eAllGraphics,
                        .srcAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentRead,
                        .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
                        .dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
                        .oldLayout = vk::ImageLayout::eDepthReadOnlyOptimal,
                        .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                        .image = viewport_->getDepthImage().getImage(),
                        .subresourceRange{
                                          .aspectMask = vk::ImageAspectFlagBits::eDepth,
                                          .baseMipLevel = 0,
                                          .levelCount = vk::RemainingMipLevels,
                                          .baseArrayLayer = 0,
                                          .layerCount = vk::RemainingArrayLayers
                        }
                };

                vk::DependencyInfo dependencyInfo{
                        .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &depthImageBarrier
                };
                frame.commandBuffer.pipelineBarrier2(dependencyInfo);
            }

            // Screen-space Ambient Occlusion pass.
            {
                std::vector<vk::DescriptorSet> descSets{aoDescriptorSet_};
                aoPass_.render(
                        AOPass::RenderInfo{
                                .commandBuffer = frame.commandBuffer,

                                .viewportExtent = viewport_->getExtent(),
                                .descriptorSets = descSets,
                                .color = RenderAttachment{.image = aoImage_.getImage(), .imageView = aoImageView_},
                                .pushConstants =
                                        AOPass::PushConstants{
                                                          .projection = camera.getProjectionMatrix(),
                                                          .nearPlane = camera.near,
                                                          .farPlane = camera.far
                                        }
                }
                );
            }

            // Transition draw image
            vk::ImageMemoryBarrier2 drawImageBarrier{
                    .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                    .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                    .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
                    // TODO: is this correct?
                    .dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
                    .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
                    .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                    .image = drawImage.getImage(),
                    .subresourceRange{
                                      .aspectMask = vk::ImageAspectFlagBits::eColor,
                                      .baseMipLevel = 0,
                                      .levelCount = vk::RemainingMipLevels,
                                      .baseArrayLayer = 0,
                                      .layerCount = vk::RemainingArrayLayers
                    }
            };

            vk::DependencyInfo dependencyInfo{.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &drawImageBarrier};
            frame.commandBuffer.pipelineBarrier2(dependencyInfo);

            // Transition swapchain image layout to PRESENT_SRC before
            // presenting
            transitionImage(
                    frame.commandBuffer, image.image, vk::ImageLayout::eColorAttachmentOptimal,
                    vk::ImageLayout::ePresentSrcKHR
            );

            // Composite pass.
            std::vector<vk::DescriptorSet> descSets{compositeDescriptorSet_};
            compositePass_.render(
                    CompositePass::RenderInfo{
                            .commandBuffer = frame.commandBuffer,
                            .viewportExtent = viewport_->getExtent(),
                            .descriptorSets = descSets,
                            .color = RenderAttachment{.image = image.image, .imageView = image.imageView},
            }
            );

            frame.commandBuffer.end();

            vk::Semaphore waitSemaphores[]{*frame.imageAvailable};
            vk::PipelineStageFlags waitStages[]{vk::PipelineStageFlagBits::eColorAttachmentOutput};

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
                        .addBinding(
                                vk::DescriptorSetLayoutBinding{
                                        .binding = 0,
                                        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                                        .descriptorCount = 1,
                                        .stageFlags = vk::ShaderStageFlagBits::eFragment
                                }
                        )
                        .build(
                                vk::DescriptorSetLayoutBindingFlagsCreateInfo{
                                        .bindingCount = 0, .pBindingFlags = nullptr
                                },
                                vk::DescriptorSetLayoutCreateFlags{}
                        );

        std::vector poolSizes{
                vk::DescriptorPoolSize{.type = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 1}
        };

        vk::DescriptorPoolCreateInfo poolInfo{
                .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
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

        std::vector descriptorSetLayouts = {*skyboxDescriptorSetLayout_};

        std::array formats = {viewport_->getDrawImageFormat()};

        // Update descriptor set.
        const vk::DescriptorImageInfo descImageInfo{
                .sampler = *cubemapSampler_,
                .imageView = *cubemapImageView_,
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
                                               .pImageInfo = &descImageInfo
                        }
        },
                {}
        );

        skyboxPass_ = SkyboxPass(
                SkyboxPass::CreateInfo{
                        .device = device_,
                        .colorAttachmentFormats = formats,
                        .descriptorSetLayouts = descriptorSetLayouts,
                        .depthAttachmentFormat = viewport_->getDepthFormat()
                }
        );
    }

    void Renderer::initAOPassResources() {
        // Create noise texture.
        constexpr size_t textureWidth = 4;

        std::vector<unsigned char> pixels;
        pixels.reserve(textureWidth * textureWidth * 4);

        ImageData imageData{
                .pixels = pixels.data(),
                .width = textureWidth,
                .height = textureWidth,
                .numChannels = 4,
                .format = vk::Format::eR8G8B8A8Srgb
        };

        std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // random floats between [0.0, 1.0]
        std::default_random_engine generator;
        for (size_t i = 0; i < (textureWidth * textureWidth); i++) {
            imageData.pixels[i * 4] = glm::packUnorm4x8(
                    glm::vec4(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0, 0.0, 1.0)
            );
        }

        aoNoiseImage_ = createImageFromData(*device_, imageData);
        aoNoiseImageView_ = device_->createImageView(
                *aoNoiseImage_.getImage(), vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor,
                aoNoiseImage_.getMipLevels()
        );
        aoNoiseSampler_ = device_->getDevice().createSampler(
                vk::SamplerCreateInfo{
                        .magFilter = vk::Filter::eNearest,
                        .minFilter = vk::Filter::eNearest,
                        .mipmapMode = vk::SamplerMipmapMode::eLinear,
                        .addressModeU = vk::SamplerAddressMode::eRepeat,
                        .addressModeV = vk::SamplerAddressMode::eRepeat,
                        .addressModeW = vk::SamplerAddressMode::eRepeat,
                        .mipLodBias = 0.0f,
                        .anisotropyEnable = vk::True,
                        .maxAnisotropy = device_->getPhysicalDevice().getProperties().limits.maxSamplerAnisotropy,
                        .compareEnable = vk::False,
                        .compareOp = vk::CompareOp::eAlways,
                        .minLod = 0.0f,
                        .maxLod = 0.0f,
                        .borderColor = vk::BorderColor::eIntOpaqueBlack,
                        .unnormalizedCoordinates = vk::False,
                }
        );

        // Create ao attachment resources
        aoImage_ =
                Image(&device_->allocator(),
                      ImageCreateInfo{
                              .width = viewport_->getExtent().width,
                              .height = viewport_->getExtent().height,
                              .format = aoFormat_,
                              .tiling = vk::ImageTiling::eOptimal,
                              .usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
                              .properties = vk::MemoryPropertyFlagBits::eDeviceLocal
                      });

        aoImageView_ = device_->createImageView(
                // TODO: is this the right aspect flag?
                *aoImage_.getImage(), aoFormat_, vk::ImageAspectFlagBits::eColor
        );

        device_->submitImmediateCommands([this](const vk::raii::CommandBuffer& commandBuffer) {
            vk::ImageMemoryBarrier2 imageMemoryBarrier{
                    .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
                    .srcAccessMask = vk::AccessFlagBits2::eNone,
                    .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                    .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                    .oldLayout = vk::ImageLayout::eUndefined,
                    .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
                    .image = *aoImage_.getImage(),
                    .subresourceRange{
                                      .aspectMask = vk::ImageAspectFlagBits::eColor,
                                      .baseMipLevel = 0,
                                      .levelCount = vk::RemainingMipLevels,
                                      .baseArrayLayer = 0,
                                      .layerCount = vk::RemainingArrayLayers
                    },
            };

            vk::DependencyInfo dependencyInfo{
                    .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &imageMemoryBarrier
            };
            commandBuffer.pipelineBarrier2(dependencyInfo);
        });

        // Create descriptor set/layout.
        DescriptorLayoutBuilder layoutBuilder(device_);

        aoDescriptorSetLayout_ = layoutBuilder
                                         .addBinding(
                                                 vk::DescriptorSetLayoutBinding{
                                                         .binding = 0,
                                                         .descriptorType = vk::DescriptorType::eSampledImage,
                                                         .descriptorCount = 1,
                                                         .stageFlags = vk::ShaderStageFlagBits::eFragment
                                                 }
                                         )
                                         .addBinding(
                                                 vk::DescriptorSetLayoutBinding{
                                                         .binding = 1,
                                                         .descriptorType = vk::DescriptorType::eSampledImage,
                                                         .descriptorCount = 1,
                                                         .stageFlags = vk::ShaderStageFlagBits::eFragment
                                                 }
                                         )
                                         .addBinding(
                                                 vk::DescriptorSetLayoutBinding{
                                                         .binding = 2,
                                                         .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                                                         .descriptorCount = 1,
                                                         .stageFlags = vk::ShaderStageFlagBits::eFragment
                                                 }
                                         )
                                         .build(
                                                 vk::DescriptorSetLayoutBindingFlagsCreateInfo{
                                                         .bindingCount = 0, .pBindingFlags = nullptr
                                                 },
                                                 vk::DescriptorSetLayoutCreateFlags{}
                                         );

        std::vector<vk::DescriptorPoolSize> poolSizes{
                vk::DescriptorPoolSize{        .type = vk::DescriptorType::eSampledImage, .descriptorCount = 2},
                vk::DescriptorPoolSize{.type = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 1}
        };

        vk::DescriptorPoolCreateInfo poolInfo{
                .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
                .maxSets = 1,
                .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
                .pPoolSizes = poolSizes.data(),
        };

        aoDescriptorPool_ = device_->getDevice().createDescriptorPool(poolInfo);

        vk::DescriptorSetAllocateInfo allocInfo{
                .descriptorPool = *aoDescriptorPool_, .descriptorSetCount = 1, .pSetLayouts = &*aoDescriptorSetLayout_
        };

        vk::raii::DescriptorSets sets(device_->getDevice(), allocInfo);
        aoDescriptorSet_ = vk::raii::DescriptorSet(std::move(sets[0]));

        std::vector<vk::DescriptorSetLayout> descriptorSetLayouts = {*aoDescriptorSetLayout_};

        // Update descriptor set.
        vk::DescriptorImageInfo depthImageInfo{
                .sampler = nullptr,
                .imageView = *viewport_->getDepthImageView(),
                .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        };

        vk::DescriptorImageInfo normalImageInfo{
                .sampler = nullptr,
                .imageView = *normalImageView_,
                .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        };

        vk::DescriptorImageInfo noiseImageInfo{
                .sampler = aoNoiseSampler_,
                .imageView = *aoNoiseImageView_,
                .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
        };

        device_->getDevice().updateDescriptorSets(
                {
                        vk::WriteDescriptorSet{
                                               .dstSet = *aoDescriptorSet_,
                                               .dstBinding = 0,
                                               .dstArrayElement = 0,
                                               .descriptorCount = 1,
                                               .descriptorType = vk::DescriptorType::eSampledImage,
                                               .pImageInfo = &depthImageInfo },
                        vk::WriteDescriptorSet{
                                               .dstSet = *aoDescriptorSet_,
                                               .dstBinding = 1,
                                               .dstArrayElement = 0,
                                               .descriptorCount = 1,
                                               .descriptorType = vk::DescriptorType::eSampledImage,
                                               .pImageInfo = &normalImageInfo},
                        vk::WriteDescriptorSet{
                                               .dstSet = *aoDescriptorSet_,
                                               .dstBinding = 2,
                                               .dstArrayElement = 0,
                                               .descriptorCount = 1,
                                               .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                                               .pImageInfo = &noiseImageInfo },
        },
                {}
        );

        std::vector<vk::Format> colorAttachmentFormats{aoFormat_};
        std::vector<vk::PushConstantRange> pushConstantRanges{
                vk::PushConstantRange{
                                      .stageFlags = vk::ShaderStageFlagBits::eFragment,
                                      .offset = 0,
                                      .size = sizeof(AOPass::PushConstants),
                                      }
        };

        aoPass_ =
                AOPass(AOPass::CreateInfo{
                        .device = device_,
                        .descriptorSetLayouts = descriptorSetLayouts,
                        .pushConstantRanges = pushConstantRanges,
                        .colorAttachmentFormats = colorAttachmentFormats,
                });
    }
    void Renderer::initCubemapPassResources() {
        cubemapImage_ =
                Image(&device_->allocator(),
                      ImageCreateInfo{
                              // TODO: magic number used for width and height
                              .width = static_cast<uint32_t>(512),
                              .height = static_cast<uint32_t>(512),
                              .format = vk::Format::eR16G16B16A16Sfloat,
                              .tiling = vk::ImageTiling::eOptimal,
                              .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment,
                              .properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
                              .mipLevels = 1,
                              .arrayLayers = 6
                      });

        device_->submitImmediateCommands([this](const vk::raii::CommandBuffer& commandBuffer) {
            // Transition to color attachment
            const vk::ImageMemoryBarrier2 imageMemoryBarrier{
                    .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
                    .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                    .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                    .oldLayout = vk::ImageLayout::eUndefined,
                    .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
                    .image = *cubemapImage_.getImage(),
                    .subresourceRange =
                            {.aspectMask = vk::ImageAspectFlagBits::eColor,
                                               .baseMipLevel = 0,
                                               .levelCount = vk::RemainingMipLevels,
                                               .baseArrayLayer = 0,
                                               .layerCount = vk::RemainingArrayLayers}
            };

            const vk::DependencyInfo dependencyInfo{
                    .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &imageMemoryBarrier
            };
            commandBuffer.pipelineBarrier2(dependencyInfo);
        });

        cubemapImageView_ = device_->getDevice().createImageView(
                vk::ImageViewCreateInfo{
                        .image = cubemapImage_.getImage(),
                        .viewType = vk::ImageViewType::eCube,
                        .format = vk::Format::eR16G16B16A16Sfloat,
                        .subresourceRange =
                                {.aspectMask = vk::ImageAspectFlagBits::eColor,
                                                   .baseMipLevel = 0,
                                                   .levelCount = vk::RemainingMipLevels,
                                                   .baseArrayLayer = 0,
                                                   .layerCount = 6}
        }
        );

        cubemapSampler_ = device_->getDevice().createSampler(
                vk::SamplerCreateInfo{
                        .magFilter = vk::Filter::eLinear,
                        .minFilter = vk::Filter::eLinear,
                        .mipmapMode = vk::SamplerMipmapMode::eLinear,
                        .addressModeU = vk::SamplerAddressMode::eRepeat,
                        .addressModeV = vk::SamplerAddressMode::eRepeat,
                        .addressModeW = vk::SamplerAddressMode::eRepeat,
                        .mipLodBias = 0.0F,
                        .anisotropyEnable = vk::True,
                        .maxAnisotropy = device_->getPhysicalDevice().getProperties().limits.maxSamplerAnisotropy,
                        .compareEnable = vk::False,
                        .compareOp = vk::CompareOp::eAlways,
                        .minLod = 0.0F,
                        .maxLod = 0.0F,
                        .borderColor = vk::BorderColor::eIntOpaqueBlack,
                        .unnormalizedCoordinates = vk::False,
                }
        );

        cubemapPass_ = CubemapPass(
                CubemapPass::CreateInfo{
                        .device = device_,
                        .descriptorSetLayouts = {},
                        .colorAttachmentFormat = vk::Format::eR16G16B16A16Sfloat
                }
        );
    }

    void Renderer::initCompositePassResources() {
        // Create descriptor set/layout.
        DescriptorLayoutBuilder layoutBuilder(device_);

        compositeDescriptorSetLayout_ = layoutBuilder
                                                .addBinding(
                                                        vk::DescriptorSetLayoutBinding{
                                                                .binding = 0,
                                                                .descriptorType = vk::DescriptorType::eSampledImage,
                                                                .descriptorCount = 1,
                                                                .stageFlags = vk::ShaderStageFlagBits::eFragment
                                                        }
                                                )
                                                .build(
                                                        vk::DescriptorSetLayoutBindingFlagsCreateInfo{
                                                                .bindingCount = 0, .pBindingFlags = nullptr
                                                        },
                                                        vk::DescriptorSetLayoutCreateFlags{}
                                                );

        std::vector<vk::DescriptorPoolSize> poolSizes{
                vk::DescriptorPoolSize{.type = vk::DescriptorType::eSampledImage, .descriptorCount = 1}
        };

        vk::DescriptorPoolCreateInfo poolInfo{
                .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
                .maxSets = 1,
                .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
                .pPoolSizes = poolSizes.data(),
        };

        compositeDescriptorPool_ = device_->getDevice().createDescriptorPool(poolInfo);

        vk::DescriptorSetAllocateInfo allocInfo{
                .descriptorPool = *compositeDescriptorPool_,
                .descriptorSetCount = 1,
                .pSetLayouts = &*compositeDescriptorSetLayout_
        };

        vk::raii::DescriptorSets sets(device_->getDevice(), allocInfo);
        compositeDescriptorSet_ = vk::raii::DescriptorSet(std::move(sets[0]));

        std::vector<vk::DescriptorSetLayout> descriptorSetLayouts = {*compositeDescriptorSetLayout_};

        // Update descriptor set.
        vk::DescriptorImageInfo descImageInfo{
                .sampler = nullptr,
                .imageView = *viewport_->getDrawImageView(),
                .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        };

        device_->getDevice().updateDescriptorSets(
                {
                        vk::WriteDescriptorSet{
                                               .dstSet = *compositeDescriptorSet_,
                                               .dstBinding = 0,
                                               .dstArrayElement = 0,
                                               .descriptorCount = 1,
                                               .descriptorType = vk::DescriptorType::eSampledImage,
                                               .pImageInfo = &descImageInfo
                        }
        },
                {}
        );

        std::vector<vk::Format> colorAttachmentFormats{viewport_->getSwapChainImageFormat()};

        compositePass_ = CompositePass(
                CompositePass::CreateInfo{
                        .device = device_,
                        .descriptorSetLayouts = descriptorSetLayouts,
                        .pushConstantRanges = {},
                        .colorAttachmentFormats = colorAttachmentFormats,
                }
        );
    }

    void Renderer::createNormalAttachment() {
        normalImage_ =
                Image(&device_->allocator(),
                      ImageCreateInfo{
                              .width = viewport_->getExtent().width,
                              .height = viewport_->getExtent().height,
                              .format = normalFormat_,
                              .tiling = vk::ImageTiling::eOptimal,
                              // TODO: check if this should be a sampled image or an input
                              // attachment.
                              .usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
                              .properties = vk::MemoryPropertyFlagBits::eDeviceLocal
                      });

        normalImageView_ =
                device_->createImageView(*normalImage_.getImage(), normalFormat_, vk::ImageAspectFlagBits::eColor);

        device_->submitImmediateCommands([this](const vk::raii::CommandBuffer& commandBuffer) {
            vk::ImageMemoryBarrier2 imageMemoryBarrier{
                    .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
                    .srcAccessMask = vk::AccessFlagBits2::eNone,
                    .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                    .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                    .oldLayout = vk::ImageLayout::eUndefined,
                    .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
                    .image = *normalImage_.getImage(),
                    .subresourceRange{
                                      .aspectMask = vk::ImageAspectFlagBits::eColor,
                                      .baseMipLevel = 0,
                                      .levelCount = vk::RemainingMipLevels,
                                      .baseArrayLayer = 0,
                                      .layerCount = vk::RemainingArrayLayers
                    },
            };

            const vk::DependencyInfo dependencyInfo{
                    .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &imageMemoryBarrier
            };
            commandBuffer.pipelineBarrier2(dependencyInfo);
        });
    }
}
