#include "renderer/renderer.h"
#include <cassert>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <stb_image.h>
#include <random>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <implot.h>
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

    Renderer::Renderer(const Window& window, std::string_view gltfPath) : window_(window) {
        instance_ = Instance{context_};

        VkSurfaceKHR tmp;
        if (glfwCreateWindowSurface(*instance_.getInstance(), window_.getWindow(), nullptr, &tmp) != VK_SUCCESS) {
            UB_ERROR("Unable to create window surface!");
        }

        surface_ = std::make_shared<vk::raii::SurfaceKHR>(instance_.getInstance(), tmp);
        device_ = std::make_shared<Device>(instance_.getInstance(), *surface_);
        viewport_ = std::make_shared<Viewport>(surface_, device_);
        imguiManager_ = ImguiManager{instance_, *device_, window_, *viewport_};

        materialManager_ = MaterialManager(device_);

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
        initIrradianceMapPassResources();
        initPrefilterMapPassResources();
        initBRDFLUTPassResources();
        initSkybox();
        initCompositePassResources();
        initTextureManager();

        // asset_ = GLTFAsset(*device_, textureManager_, materialManager_,
        // "assets/DamagedHelmet/glTF/DamagedHelmet.gltf");

        /*
        asset_ = GLTFAsset(
                *device_, textureManager_, materialManager_, "assets/ABeautifulGame/glTF/ABeautifulGame.gltf"
        );
        */

        asset_ = GLTFAsset(*device_, textureManager_, materialManager_, gltfPath);

        {
            std::vector setLayouts{*iblDescriptorSetLayout_, *textureDescriptorSetLayout_};
            depthPass_ = DepthPass(device_, viewport_, setLayouts);
        }

        // Create lighting pass.
        std::vector setLayouts = {*iblDescriptorSetLayout_, *textureDescriptorSetLayout_};

        std::vector pushConstantRanges = {
            vk::PushConstantRange{
                                  .stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                                  .offset = 0,
                                  .size = sizeof(PushConstants),
                                  }
        };

        std::array formats{
            // TODO: reevaluate normal format, maybe Snorm?
            viewport_->getDrawImageFormat(), viewport_->getNormalImageFormat()
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

        // TODO: Implement job queue instead of using immediate commands.
        generateEnvironmentMap();
        generateIrradianceMap();
        generatePrefilterMap();
        generateBRDFLUT();
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
        // TODO: move this fetch outside of render loop.
        const auto timestampPeriod = device_->getPhysicalDevice().getProperties2().properties.limits.timestampPeriod;
        updateScene(camera);

        // TODO: move to ImguiManager somehow
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        const auto createImguiRenderData = [&](uint64_t gpuTimestamp) {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            const auto basePos = viewport->WorkPos;

            ImGui::Begin("Average FPS");
            ImGui::Text("Average FPS: %d", static_cast<uint32_t>(state.averageFPS));
            ImGui::End();

            ImGui::Begin("Frame Statistics");
            ImGui::Text("CPU: %f ms", 1.0f / state.averageFPS * 1000.0f);
            ImGui::Text("GPU: %f ms", static_cast<float>(gpuTimestamp) * timestampPeriod / 1000000.0f);
            ImGui::End();

            ImGui::Render();
        };

        // TODO: fix formatted lambda args causing misaligned indents
        viewport_->doFrame([this, &camera, createImguiRenderData, timestampPeriod](
                               Frame& frame, const SwapchainImage& image, const Image& drawImage,
                               const vk::raii::ImageView& drawImageView
                           ) {
            vk::CommandBufferBeginInfo beginInfo{};
            frame.commandBuffer.begin(beginInfo);
            frame.commandBuffer.resetQueryPool(frame.timestampQueryPool, 0, 2);
            if (frame.timestamps[1] != 0) {
                frame.commandBuffer.writeTimestamp2(
                    vk::PipelineStageFlagBits2::eTopOfPipe, frame.timestampQueryPool, 0
                );
            }

            // Transition swapchain image layout to GENERAL before rendering
            transitionImage(frame.commandBuffer, image.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);

            std::vector<vk::DescriptorSet> descriptorSets{*iblDescriptorSet_, *textureDescriptorSet_};

            // Depth pre-pass
            depthPass_.render(frame.commandBuffer, drawContext_, sceneDataBuffer_, descriptorSets);

            // Transition draw image.
            {
                vk::ImageMemoryBarrier2 imageMemoryBarrier{
                    .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
                    .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                    .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                    .oldLayout = vk::ImageLayout::eUndefined,
                    .newLayout = vk::ImageLayout::eGeneral,
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
                    .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
                    .srcAccessMask = vk::AccessFlagBits2::eNone,
                    .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                    .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                    .oldLayout = vk::ImageLayout::eUndefined,
                    .newLayout = vk::ImageLayout::eGeneral,
                    .image = *viewport_->getNormalImage().getImage(),
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
                    .color = RenderAttachment{.image = drawImage.getImage(),.imageView = drawImageView                                                                                             },
                    .normal =
                        RenderAttachment{
                                              .image = viewport_->getNormalImage().getImage(),
                                              .imageView = viewport_->getNormalImageView()                                               },
                    .depth = RenderAttachment{
                                              .image = viewport_->getDepthImage().getImage(), .imageView = viewport_->getDepthImageView()}
            }
            );

            // Skybox pass.
            {
                std::vector descriptorSets{*skyboxDescriptorSet_};

                const auto viewProjection = camera.getProjectionMatrix() * glm::mat4(glm::mat3(camera.getViewMatrix()));
                skyboxPass_.render(
                    SkyboxPass::RenderInfo{
                        .commandBuffer = frame.commandBuffer,
                        .viewportExtent = viewport_->getExtent(),
                        .descriptorSets = {descriptorSets},
                        .color = RenderAttachment{.image = drawImage.getImage(), .imageView = drawImageView},
                        .depth =
                            RenderAttachment{
                                           .image = viewport_->getDepthImage().getImage(),
                                           .imageView = viewport_->getDepthImageView()
                            },
                        .pushConstants = SkyboxPass::PushConstants{.viewProjection = viewProjection},
                }
                );
            }


            // Transition AO image.
            {
                vk::ImageMemoryBarrier2 aoImageBarrier{
                    .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
                    .srcAccessMask = vk::AccessFlagBits2::eNone,
                    .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                    .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                    .oldLayout = vk::ImageLayout::eUndefined,
                    .newLayout = vk::ImageLayout::eGeneral,
                    .image = viewport_->getAOImage().getImage(),
                    .subresourceRange{
                                      .aspectMask = vk::ImageAspectFlagBits::eColor,
                                      .baseMipLevel = 0,
                                      .levelCount = vk::RemainingMipLevels,
                                      .baseArrayLayer = 0,
                                      .layerCount = vk::RemainingArrayLayers
                    },
                };

                vk::DependencyInfo dependencyInfo{
                    .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &aoImageBarrier
                };
                frame.commandBuffer.pipelineBarrier2(dependencyInfo);
            }

            // Screen-space Ambient Occlusion pass.
            {
                // Update descriptor set in case the viewport is rebuilt
                // PERF: only do this when swapchain is rebuilt
                vk::DescriptorImageInfo depthImageInfo{
                    .sampler = nullptr,
                    .imageView = *viewport_->getDepthImageView(),
                    .imageLayout = vk::ImageLayout::eGeneral,
                };
                vk::DescriptorImageInfo normalImageInfo{
                    .sampler = nullptr,
                    .imageView = *viewport_->getNormalImageView(),
                    .imageLayout = vk::ImageLayout::eGeneral,
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
                },
                    {}
                );
                std::vector<vk::DescriptorSet> descSets{aoDescriptorSet_};
                aoPass_.render(
                    AOPass::RenderInfo{
                        .commandBuffer = frame.commandBuffer,

                        .viewportExtent = viewport_->getExtent(),
                        .descriptorSets = descSets,
                        .color =
                            RenderAttachment{
                                             .image = viewport_->getAOImage().getImage(), .imageView = viewport_->getAOImageView()
                            },
                        .pushConstants = AOPass::PushConstants{
                                             .projection = camera.getProjectionMatrix(), .nearPlane = camera.near, .farPlane = camera.far
                        }
                }
                );
            }

            // Composite pass.

            // Update descriptor set in case viewport is rebuilt
            // PERF: only do this when swapchain is rebuilt
            vk::DescriptorImageInfo descImageInfo{
                .sampler = nullptr,
                .imageView = *viewport_->getDrawImageView(),
                .imageLayout = vk::ImageLayout::eGeneral,
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

            std::vector<vk::DescriptorSet> descSets{compositeDescriptorSet_};
            compositePass_.render(
                CompositePass::RenderInfo{
                    .commandBuffer = frame.commandBuffer,
                    .viewportExtent = viewport_->getExtent(),
                    .descriptorSets = descSets,
                    .color = RenderAttachment{.image = image.image, .imageView = image.imageView},
            }
            );

            // Imgui pass
            // TODO: move to dedicated class
            std::array colorAttachmentInfos{
                vk::RenderingAttachmentInfo{
                                            .imageView = image.imageView,
                                            .imageLayout = vk::ImageLayout::eGeneral,
                                            .loadOp = vk::AttachmentLoadOp::eLoad,
                                            .storeOp = vk::AttachmentStoreOp::eStore,
                                            }
            };

            vk::RenderingInfo renderingInfo{
                .renderArea = {.offset = {0, 0}, .extent = viewport_->getExtent()},
                .layerCount = 1,
                .colorAttachmentCount = colorAttachmentInfos.size(),
                .pColorAttachments = colorAttachmentInfos.data(),
            };

            createImguiRenderData(frame.timestamps[2] - frame.timestamps[0]);


            frame.commandBuffer.beginRendering(renderingInfo);
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *frame.commandBuffer);
            frame.commandBuffer.endRendering();

            // Transition swapchain image layout to PRESENT_SRC before presenting
            transitionImage(
                frame.commandBuffer, image.image, vk::ImageLayout::eGeneral, vk::ImageLayout::ePresentSrcKHR
            );

            if (frame.timestamps[3] != 0) {
                frame.commandBuffer.writeTimestamp2(
                    vk::PipelineStageFlagBits2::eTopOfPipe, frame.timestampQueryPool, 1
                );
            }

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

            auto [result, timestamps] = frame.timestampQueryPool.getResults<uint64_t>(
                0, 2, sizeof(uint64_t) * 4, sizeof(uint64_t) * 2,
                vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWithAvailability
            );

            if (result != vk::Result::eNotReady) {
                frame.timestamps = timestamps;
            }
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
                    vk::DescriptorSetLayoutBindingFlagsCreateInfo{.bindingCount = 0, .pBindingFlags = nullptr},
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
            .imageLayout = vk::ImageLayout::eGeneral,
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
                },
        },
            {}
        );

        skyboxPass_ = SkyboxPass(
            SkyboxPass::CreateInfo{
                .device = device_,
                .descriptorSetLayouts = descriptorSetLayouts,
                .colorAttachmentFormats = formats,

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

        // Create descriptor set/layout.
        DescriptorLayoutBuilder layoutBuilder(device_);

        aoDescriptorSetLayout_ =
            layoutBuilder
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
                    vk::DescriptorSetLayoutBindingFlagsCreateInfo{.bindingCount = 0, .pBindingFlags = nullptr},
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
            .imageLayout = vk::ImageLayout::eGeneral,
        };

        vk::DescriptorImageInfo normalImageInfo{
            .sampler = nullptr,
            .imageView = *viewport_->getNormalImageView(),
            .imageLayout = vk::ImageLayout::eGeneral,
        };

        vk::DescriptorImageInfo noiseImageInfo{
            .sampler = aoNoiseSampler_, .imageView = *aoNoiseImageView_, .imageLayout = vk::ImageLayout::eGeneral
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

        std::vector colorAttachmentFormats{viewport_->getAOImageFormat()};
        std::vector pushConstantRanges{
            vk::PushConstantRange{
                                  .stageFlags = vk::ShaderStageFlagBits::eFragment,
                                  .offset = 0,
                                  .size = sizeof(AOPass::PushConstants),
                                  }
        };

        aoPass_ = AOPass(
            AOPass::CreateInfo{
                .device = device_,
                .descriptorSetLayouts = descriptorSetLayouts,
                .pushConstantRanges = pushConstantRanges,
                .colorAttachmentFormats = colorAttachmentFormats,
            }
        );
    }
    void Renderer::initCubemapPassResources() {
        // Load HDR equirectangular map image file.
        int width, height, nrComponents;
        float* data = stbi_loadf("assets/skybox/newport_loft.hdr", &width, &height, &nrComponents, 4);
        if (data == nullptr) {
            UB_ERROR("Failed to load texture image");
        }


        const vk::DeviceSize imageSize = width * height * 4 * 4;
        const vk::BufferCreateInfo stagingBufferCreateInfo{
            .size = imageSize, .usage = vk::BufferUsageFlagBits::eTransferSrc
        };

        VmaAllocationCreateInfo stagingBufferAllocCreateInfo{
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
        };

        const Buffer stagingBuffer = device_->createBuffer(stagingBufferCreateInfo, stagingBufferAllocCreateInfo);

        auto pixels = std::span{data, static_cast<size_t>(width * height * 4)};
        std::ranges::copy(pixels, static_cast<float*>(stagingBuffer.getMappedMemory()));
        stbi_image_free(data);

        // Create equirectangular map image.
        equirectangularMapImage_ = Image(
            &device_->allocator(),
            ImageCreateInfo{
                .width = static_cast<uint32_t>(width),
                .height = static_cast<uint32_t>(height),
                .format = vk::Format::eR32G32B32A32Sfloat,
                .tiling = vk::ImageTiling::eOptimal,
                .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc |
                         vk::ImageUsageFlagBits::eTransferDst,
                .properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
                .mipLevels = 1
            }
        );

        // Upload data to image.
        device_->submitImmediateCommands([this, &stagingBuffer, width,
                                          height](const vk::raii::CommandBuffer& commandBuffer) {
            transitionImage(
                commandBuffer, *equirectangularMapImage_.getImage(), vk::ImageLayout::eUndefined,
                vk::ImageLayout::eGeneral
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
                                               .layerCount = 1
                    },
                .imageOffset = {0, 0, 0},
                .imageExtent = vk::Extent3D{
                                               .width = static_cast<uint32_t>(width), .height = static_cast<uint32_t>(height), .depth = 1
                }
            };
            commandBuffer.copyBufferToImage(
                *stagingBuffer.getBuffer(), *equirectangularMapImage_.getImage(), vk::ImageLayout::eGeneral,
                {copyRegion}
            );
        });

        equirectangularMapImageView_ = device_->getDevice().createImageView(
            vk::ImageViewCreateInfo{
                .image = equirectangularMapImage_.getImage(),
                .viewType = vk::ImageViewType::e2D,
                .format = vk::Format::eR32G32B32A32Sfloat,
                .subresourceRange = {
                                     .aspectMask = vk::ImageAspectFlagBits::eColor,
                                     .baseMipLevel = 0,
                                     .levelCount = vk::RemainingMipLevels,
                                     .baseArrayLayer = 0,
                                     .layerCount = 1
                }
        }
        );

        equirectangularMapSampler_ = device_->getDevice().createSampler(
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

        // Create descriptor set/layout.
        DescriptorLayoutBuilder layoutBuilder(device_);

        cubemapDescriptorSetLayout_ =
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
                    vk::DescriptorSetLayoutBindingFlagsCreateInfo{.bindingCount = 0, .pBindingFlags = nullptr},
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

        cubemapDescriptorPool_ = device_->getDevice().createDescriptorPool(poolInfo);

        vk::DescriptorSetAllocateInfo allocInfo{
            .descriptorPool = *cubemapDescriptorPool_,
            .descriptorSetCount = 1,
            .pSetLayouts = &*cubemapDescriptorSetLayout_
        };

        vk::raii::DescriptorSets sets(device_->getDevice(), allocInfo);
        cubemapDescriptorSet_ = vk::raii::DescriptorSet(std::move(sets[0]));

        std::vector descriptorSetLayouts = {*cubemapDescriptorSetLayout_};

        // Update descriptor set.
        const vk::DescriptorImageInfo descImageInfo{
            .sampler = *equirectangularMapSampler_,
            .imageView = *equirectangularMapImageView_,
            .imageLayout = vk::ImageLayout::eGeneral,
        };

        // TODO: share descriptor set with skybox pipeline/pass?
        device_->getDevice().updateDescriptorSets(
            {
                vk::WriteDescriptorSet{
                                       .dstSet = *cubemapDescriptorSet_,
                                       .dstBinding = 0,
                                       .dstArrayElement = 0,
                                       .descriptorCount = 1,
                                       .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                                       .pImageInfo = &descImageInfo
                }
        },
            {}
        );

        // Create cubemap image.
        cubemapImage_ = Image(
            &device_->allocator(),
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
            }
        );

        device_->submitImmediateCommands([this](const vk::raii::CommandBuffer& commandBuffer) {
            // Transition to color attachment
            const vk::ImageMemoryBarrier2 imageMemoryBarrier{
                .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
                .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                .oldLayout = vk::ImageLayout::eUndefined,
                .newLayout = vk::ImageLayout::eGeneral,
                .image = *cubemapImage_.getImage(),
                .subresourceRange = {
                                     .aspectMask = vk::ImageAspectFlagBits::eColor,
                                     .baseMipLevel = 0,
                                     .levelCount = vk::RemainingMipLevels,
                                     .baseArrayLayer = 0,
                                     .layerCount = vk::RemainingArrayLayers
                }
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
                .subresourceRange = {
                                     .aspectMask = vk::ImageAspectFlagBits::eColor,
                                     .baseMipLevel = 0,
                                     .levelCount = vk::RemainingMipLevels,
                                     .baseArrayLayer = 0,
                                     .layerCount = 6
                }
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
                .descriptorSetLayouts = descriptorSetLayouts,
                .colorAttachmentFormat = vk::Format::eR16G16B16A16Sfloat
            }
        );
    }

    void Renderer::initCompositePassResources() {
        // Create descriptor set/layout.
        DescriptorLayoutBuilder layoutBuilder(device_);

        compositeDescriptorSetLayout_ =
            layoutBuilder
                .addBinding(
                    vk::DescriptorSetLayoutBinding{
                        .binding = 0,
                        .descriptorType = vk::DescriptorType::eSampledImage,
                        .descriptorCount = 1,
                        .stageFlags = vk::ShaderStageFlagBits::eFragment
                    }
                )
                .build(
                    vk::DescriptorSetLayoutBindingFlagsCreateInfo{.bindingCount = 0, .pBindingFlags = nullptr},
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
            .imageLayout = vk::ImageLayout::eGeneral,
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

    void Renderer::initIrradianceMapPassResources() {
        // Create descriptor set/layout.
        DescriptorLayoutBuilder layoutBuilder(device_);

        irradianceMapDescriptorSetLayout_ =
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
                    vk::DescriptorSetLayoutBindingFlagsCreateInfo{.bindingCount = 0, .pBindingFlags = nullptr},
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
        irradianceMapDescriptorPool_ = device_->getDevice().createDescriptorPool(poolInfo);

        vk::DescriptorSetAllocateInfo allocInfo{
            .descriptorPool = *irradianceMapDescriptorPool_,
            .descriptorSetCount = 1,
            .pSetLayouts = &*irradianceMapDescriptorSetLayout_
        };

        vk::raii::DescriptorSets sets(device_->getDevice(), allocInfo);
        irradianceMapDescriptorSet_ = vk::raii::DescriptorSet(std::move(sets[0]));

        std::vector descriptorSetLayouts = {*irradianceMapDescriptorSetLayout_};

        // Update descriptor set.
        const vk::DescriptorImageInfo descImageInfo{
            .sampler = *cubemapSampler_,
            .imageView = *cubemapImageView_,
            .imageLayout = vk::ImageLayout::eGeneral,
        };

        // TODO: share descriptor set with skybox pipeline/pass?
        device_->getDevice().updateDescriptorSets(
            {
                vk::WriteDescriptorSet{
                                       .dstSet = *irradianceMapDescriptorSet_,
                                       .dstBinding = 0,
                                       .dstArrayElement = 0,
                                       .descriptorCount = 1,
                                       .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                                       .pImageInfo = &descImageInfo
                }
        },
            {}
        );

        // Create irradiance map image.
        // Create cubemap image.
        irradianceMapImage_ = Image(
            &device_->allocator(),
            ImageCreateInfo{
                // TODO: magic number used for width and height
                .width = static_cast<uint32_t>(32),
                .height = static_cast<uint32_t>(32),
                .format = vk::Format::eR16G16B16A16Sfloat,
                .tiling = vk::ImageTiling::eOptimal,
                .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment,
                .properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
                .mipLevels = 1,
                .arrayLayers = 6
            }
        );

        device_->submitImmediateCommands([this](const vk::raii::CommandBuffer& commandBuffer) {
            // Transition to color attachment
            const vk::ImageMemoryBarrier2 imageMemoryBarrier{
                .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
                .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                .oldLayout = vk::ImageLayout::eUndefined,
                .newLayout = vk::ImageLayout::eGeneral,
                .image = *irradianceMapImage_.getImage(),
                .subresourceRange = {
                                     .aspectMask = vk::ImageAspectFlagBits::eColor,
                                     .baseMipLevel = 0,
                                     .levelCount = vk::RemainingMipLevels,
                                     .baseArrayLayer = 0,
                                     .layerCount = vk::RemainingArrayLayers
                }
            };

            const vk::DependencyInfo dependencyInfo{
                .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &imageMemoryBarrier
            };
            commandBuffer.pipelineBarrier2(dependencyInfo);
        });

        irradianceMapImageView_ = device_->getDevice().createImageView(
            vk::ImageViewCreateInfo{
                .image = irradianceMapImage_.getImage(),
                .viewType = vk::ImageViewType::eCube,
                .format = vk::Format::eR16G16B16A16Sfloat,
                .subresourceRange = {
                                     .aspectMask = vk::ImageAspectFlagBits::eColor,
                                     .baseMipLevel = 0,
                                     .levelCount = vk::RemainingMipLevels,
                                     .baseArrayLayer = 0,
                                     .layerCount = 6
                }
        }
        );

        irradianceMapSampler_ = device_->getDevice().createSampler(
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

        irradiancePass_ = IrradiancePass(
            IrradiancePass::CreateInfo{
                .device = device_,
                .descriptorSetLayouts = descriptorSetLayouts,
                .colorAttachmentFormat = vk::Format::eR16G16B16A16Sfloat
            }
        );
    }
    void Renderer::generateEnvironmentMap() const {
        device_->submitImmediateCommands([this](const vk::raii::CommandBuffer& commandBuffer) {
            // Transition cubemap image to COLOR_ATTACHMENT_OPTIMAL
            {
                vk::ImageMemoryBarrier2 imageMemoryBarrier{
                    // PERF: we really toppin da pipe with this one
                    .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
                    .srcAccessMask = vk::AccessFlagBits2::eNone,
                    .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                    .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                    .oldLayout = vk::ImageLayout::eUndefined,
                    .newLayout = vk::ImageLayout::eGeneral,
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
                commandBuffer.pipelineBarrier2(dependencyInfo);
            }

            // Equirectangular to Cubemap pass.
            {
                std::vector descriptorSets{*cubemapDescriptorSet_};
                cubemapPass_.render(
                    CubemapPass::RenderInfo{
                        .commandBuffer = commandBuffer,
                        .viewportExtent = vk::Extent2D(512, 512),
                        .descriptorSets = descriptorSets,
                        .color = RenderAttachment{.image = cubemapImage_.getImage(), .imageView = cubemapImageView_},
                }
                );
            }
        });
    }
    void Renderer::generateIrradianceMap() const {
        device_->submitImmediateCommands([this](const vk::raii::CommandBuffer& commandBuffer) {
            // Irradiance map pass.
            {
                std::vector descriptorSets{*irradianceMapDescriptorSet_};
                irradiancePass_.render(
                    IrradiancePass::RenderInfo{
                        .commandBuffer = commandBuffer,
                        .viewportExtent = vk::Extent2D(32, 32),
                        .descriptorSets = descriptorSets,
                        .color = RenderAttachment{
                                                  .image = irradianceMapImage_.getImage(), .imageView = irradianceMapImageView_
                        },
                }
                );
            }
        });
    }
    void Renderer::initPrefilterMapPassResources() {
        // Create descriptor set/layout
        DescriptorLayoutBuilder layoutBuilder(device_);
        prefilterMapDescriptorSetLayout_ =
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
                    vk::DescriptorSetLayoutBindingFlagsCreateInfo{.bindingCount = 0, .pBindingFlags = nullptr},
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

        prefilterMapDescriptorPool_ = device_->getDevice().createDescriptorPool(poolInfo);

        vk::DescriptorSetAllocateInfo allocInfo{
            .descriptorPool = *prefilterMapDescriptorPool_,
            .descriptorSetCount = 1,
            .pSetLayouts = &*prefilterMapDescriptorSetLayout_
        };

        vk::raii::DescriptorSets sets(device_->getDevice(), allocInfo);
        prefilterMapDescriptorSet_ = vk::raii::DescriptorSet(std::move(sets[0]));

        std::vector descriptorSetLayouts = {*prefilterMapDescriptorSetLayout_};

        // Update descriptor set.
        const vk::DescriptorImageInfo descImageInfo{
            .sampler = *cubemapSampler_,
            .imageView = *cubemapImageView_,
            .imageLayout = vk::ImageLayout::eGeneral,
        };

        // TODO: share descriptor set with skybox pipeline/pass?
        device_->getDevice().updateDescriptorSets(
            {
                vk::WriteDescriptorSet{
                                       .dstSet = *prefilterMapDescriptorSet_,
                                       .dstBinding = 0,
                                       .dstArrayElement = 0,
                                       .descriptorCount = 1,
                                       .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                                       .pImageInfo = &descImageInfo
                }
        },
            {}
        );

        // Create prefilter map image.
        constexpr int maxMipLevels = 5;
        constexpr int imageSize = 128;
        prefilterMapImage_ = Image(
            &device_->allocator(),
            ImageCreateInfo{
                .width = static_cast<uint32_t>(imageSize),
                .height = static_cast<uint32_t>(imageSize),
                .format = vk::Format::eR16G16B16A16Sfloat,
                .tiling = vk::ImageTiling::eOptimal,
                .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment,
                .properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
                .mipLevels = maxMipLevels,
                .arrayLayers = 6
            }
        );

        device_->submitImmediateCommands([this](const vk::raii::CommandBuffer& commandBuffer) {
            // Transition to color attachment
            const vk::ImageMemoryBarrier2 imageMemoryBarrier{
                .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
                .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                .oldLayout = vk::ImageLayout::eUndefined,
                .newLayout = vk::ImageLayout::eGeneral,
                .image = *prefilterMapImage_.getImage(),
                .subresourceRange = {
                                     .aspectMask = vk::ImageAspectFlagBits::eColor,
                                     .baseMipLevel = 0,
                                     .levelCount = vk::RemainingMipLevels,
                                     .baseArrayLayer = 0,
                                     .layerCount = vk::RemainingArrayLayers
                }
            };

            const vk::DependencyInfo dependencyInfo{
                .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &imageMemoryBarrier
            };
            commandBuffer.pipelineBarrier2(dependencyInfo);
        });

        prefilterMapImageView_ = device_->getDevice().createImageView(
            vk::ImageViewCreateInfo{
                .image = prefilterMapImage_.getImage(),
                .viewType = vk::ImageViewType::eCube,
                .format = vk::Format::eR16G16B16A16Sfloat,
                .subresourceRange = {
                                     .aspectMask = vk::ImageAspectFlagBits::eColor,
                                     .baseMipLevel = 0,
                                     .levelCount = vk::RemainingMipLevels,
                                     .baseArrayLayer = 0,
                                     .layerCount = 6
                }
        }
        );

        prefilterMapSampler_ = device_->getDevice().createSampler(
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

        prefilterPass_ = PrefilterPass(
            PrefilterPass::CreateInfo{
                .device = device_,
                .descriptorSetLayouts = descriptorSetLayouts,
                .colorAttachmentFormat = vk::Format::eR16G16B16A16Sfloat,
            }
        );
    }
    void Renderer::generatePrefilterMap() const {
        constexpr int maxMipLevels = 5;
        for (uint32_t mipLevel = 0; mipLevel < maxMipLevels; ++mipLevel) {
            constexpr uint32_t imageSize = 128;
            const uint32_t mipSize = imageSize >> mipLevel;
            const float roughness = static_cast<float>(mipLevel) / static_cast<float>(maxMipLevels - 1);

            const auto imageView = device_->getDevice().createImageView(
                vk::ImageViewCreateInfo{
                    .image = prefilterMapImage_.getImage(),
                    .viewType = vk::ImageViewType::eCube,
                    .format = vk::Format::eR16G16B16A16Sfloat,
                    .subresourceRange = {
                                         .aspectMask = vk::ImageAspectFlagBits::eColor,
                                         .baseMipLevel = mipLevel,
                                         .levelCount = 1,
                                         .baseArrayLayer = 0,
                                         .layerCount = 6
                    }
            }
            );

            // TODO: Assumes environment map is in the right layout after environment map generation.
            device_->submitImmediateCommands([this, &imageView, roughness,
                                              mipSize](const vk::raii::CommandBuffer& commandBuffer) {
                std::vector descriptorSets{*prefilterMapDescriptorSet_};
                prefilterPass_.render(
                    PrefilterPass::RenderInfo{
                        .commandBuffer = commandBuffer,
                        .viewportExtent = vk::Extent2D(mipSize, mipSize),
                        .descriptorSets = descriptorSets,
                        .color = RenderAttachment{.image = prefilterMapImage_.getImage(), .imageView = imageView},
                        .roughness = roughness
                }
                );
            });
        }
    }
    void Renderer::initBRDFLUTPassResources() {
        brdfLutMapImage_ = Image(
            &device_->allocator(),
            ImageCreateInfo{
                .width = 512,
                .height = 512,
                .format = vk::Format::eR16G16Sfloat,
                .tiling = vk::ImageTiling::eOptimal,
                .usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
                .properties = vk::MemoryPropertyFlagBits::eDeviceLocal
            }
        );
        brdfLutMapImageView_ = device_->createImageView(
            *brdfLutMapImage_.getImage(), vk::Format::eR16G16Sfloat, vk::ImageAspectFlagBits::eColor
        );

        brdfLutMapSampler_ = device_->getDevice().createSampler(
            vk::SamplerCreateInfo{
                .magFilter = vk::Filter::eLinear,
                .minFilter = vk::Filter::eLinear,
                .mipmapMode = vk::SamplerMipmapMode::eLinear,
                .addressModeU = vk::SamplerAddressMode::eClampToEdge,
                .addressModeV = vk::SamplerAddressMode::eClampToEdge,
                .addressModeW = vk::SamplerAddressMode::eClampToEdge,
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

        brdflutPass_ =
            BRDFLUTPass(BRDFLUTPass::CreateInfo{.device = device_, .colorAttachmentFormat = vk::Format::eR16G16Sfloat});
    }
    void Renderer::generateBRDFLUT() const {
        device_->submitImmediateCommands([this](const vk::raii::CommandBuffer& commandBuffer) {
            // Transition BRDFLUT map image.
            {
                const vk::ImageMemoryBarrier2 imageMemoryBarrier{
                    .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
                    .srcAccessMask = vk::AccessFlagBits2::eNone,
                    .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                    .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                    .oldLayout = vk::ImageLayout::eUndefined,
                    .newLayout = vk::ImageLayout::eGeneral,
                    .image = brdfLutMapImage_.getImage(),
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
            }

            {
                brdflutPass_.render(
                    BRDFLUTPass::RenderInfo{
                        .commandBuffer = commandBuffer,
                        .viewportExtent = vk::Extent2D(512, 512),
                        .color =
                            RenderAttachment{.image = brdfLutMapImage_.getImage(), .imageView = brdfLutMapImageView_},
                }
                );
            }
        });
    }

    void Renderer::initTextureManager() {
        // Create layout.
        DescriptorLayoutBuilder layoutBuilder(device_);

        constexpr vk::DescriptorBindingFlags bindingFlags = vk::DescriptorBindingFlagBits::eVariableDescriptorCount |
                                                            vk::DescriptorBindingFlagBits::ePartiallyBound |
                                                            vk::DescriptorBindingFlagBits::eUpdateAfterBind;

        textureDescriptorSetLayout_ =
            layoutBuilder
                .addBinding(
                    vk::DescriptorSetLayoutBinding{
                        .binding = 0,
                        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                        .descriptorCount = maxTextures,
                        .stageFlags = vk::ShaderStageFlagBits::eFragment,
                    }
                )
                .build(
                    vk::DescriptorSetLayoutBindingFlagsCreateInfo{.bindingCount = 1, .pBindingFlags = &bindingFlags},
                    vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool
                );

        {
            DescriptorLayoutBuilder layoutBuilder(device_);
            iblDescriptorSetLayout_ =
                layoutBuilder
                    .addBinding(
                        vk::DescriptorSetLayoutBinding{
                            .binding = 0,
                            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                            .descriptorCount = 1,
                            .stageFlags = vk::ShaderStageFlagBits::eFragment,
                        }
                    )
                    .addBinding(
                        vk::DescriptorSetLayoutBinding{
                            .binding = 1,
                            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                            .descriptorCount = 1,
                            .stageFlags = vk::ShaderStageFlagBits::eFragment,
                        }
                    )
                    .addBinding(
                        vk::DescriptorSetLayoutBinding{
                            .binding = 2,
                            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                            .descriptorCount = 1,
                            .stageFlags = vk::ShaderStageFlagBits::eFragment,
                        }
                    )
                    .build(
                        vk::DescriptorSetLayoutBindingFlagsCreateInfo{.bindingCount = 0, .pBindingFlags = nullptr},
                        vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool
                    );
        }

        // Create pool.
        // TODO: account for other first descriptor set properly
        std::vector poolSizes{
            vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageImage,     .descriptorCount = maxTextures                                   },
            vk::DescriptorPoolSize{       .type = vk::DescriptorType::eUniformBuffer,     .descriptorCount = maxTextures},
            vk::DescriptorPoolSize{             .type = vk::DescriptorType::eSampler,     .descriptorCount = maxTextures},
            vk::DescriptorPoolSize{
                                   .type = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = maxTextures + 3}
        };

        const vk::DescriptorPoolCreateInfo poolInfo{
            .flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind |
                     vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = maxTextures,
            .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
            .pPoolSizes = poolSizes.data()
        };

        lightingDescriptorPool_ = device_->getDevice().createDescriptorPool(poolInfo);

        // Create set.
        vk::StructureChain allocInfo{
            vk::DescriptorSetAllocateInfo{
                                          .descriptorPool = *lightingDescriptorPool_,
                                          .descriptorSetCount = 1,
                                          .pSetLayouts = &*textureDescriptorSetLayout_
            },
            vk::DescriptorSetVariableDescriptorCountAllocateInfo{
                                          .descriptorSetCount = 1, .pDescriptorCounts = &maxTextures
            }
        };

        vk::raii::DescriptorSets sets(device_->getDevice(), allocInfo.get());
        textureDescriptorSet_ = std::make_shared<vk::raii::DescriptorSet>(std::move(sets[0]));
        textureManager_ = TextureManager(device_, textureDescriptorSet_);

        {
            vk::raii::DescriptorSets sets(
                device_->getDevice(), vk::DescriptorSetAllocateInfo{
                                          .descriptorPool = *lightingDescriptorPool_,
                                          .descriptorSetCount = 1,
                                          .pSetLayouts = &*iblDescriptorSetLayout_
                                      }
            );
            iblDescriptorSet_ = std::move(sets[0]);
        }

        // Update descriptor set.
        const vk::DescriptorImageInfo irradianceDescImageInfo{
            .sampler = *irradianceMapSampler_,
            .imageView = *irradianceMapImageView_,
            .imageLayout = vk::ImageLayout::eGeneral,
        };
        const vk::DescriptorImageInfo prefilterDescImageInfo{
            .sampler = *prefilterMapSampler_,
            .imageView = *prefilterMapImageView_,
            .imageLayout = vk::ImageLayout::eGeneral,
        };
        const vk::DescriptorImageInfo brdfLutDescImageInfo{
            .sampler = *brdfLutMapSampler_,
            .imageView = *brdfLutMapImageView_,
            .imageLayout = vk::ImageLayout::eGeneral,
        };

        device_->getDevice().updateDescriptorSets(
            {
                vk::WriteDescriptorSet{
                                       .dstSet = *iblDescriptorSet_,
                                       .dstBinding = 0,
                                       .dstArrayElement = 0,
                                       .descriptorCount = 1,
                                       .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                                       .pImageInfo = &irradianceDescImageInfo},
                vk::WriteDescriptorSet{
                                       .dstSet = *iblDescriptorSet_,
                                       .dstBinding = 1,
                                       .dstArrayElement = 0,
                                       .descriptorCount = 1,
                                       .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                                       .pImageInfo = &prefilterDescImageInfo },
                vk::WriteDescriptorSet{
                                       .dstSet = *iblDescriptorSet_,
                                       .dstBinding = 2,
                                       .dstArrayElement = 0,
                                       .descriptorCount = 1,
                                       .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                                       .pImageInfo = &brdfLutDescImageInfo   }
        },
            {}
        );
    }
}
