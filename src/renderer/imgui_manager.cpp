#include "renderer/imgui_manager.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <implot.h>
#include "renderer/instance.h"
#include "renderer/device.h"
#include "renderer/viewport.h"
#include "pch.h"

namespace yuubi {

    ImguiManager::ImguiManager(
        const Instance& instance, Device& device, const Window& window, const Viewport& viewport
    ) {
        std::array<vk::DescriptorPoolSize, 11> poolSizes = {
            vk::DescriptorPoolSize{             vk::DescriptorType::eSampler, 1000},
            vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, 1000},
            vk::DescriptorPoolSize{        vk::DescriptorType::eSampledImage, 1000},
            vk::DescriptorPoolSize{        vk::DescriptorType::eStorageImage, 1000},
            vk::DescriptorPoolSize{  vk::DescriptorType::eUniformTexelBuffer, 1000},
            vk::DescriptorPoolSize{  vk::DescriptorType::eStorageTexelBuffer, 1000},
            vk::DescriptorPoolSize{       vk::DescriptorType::eUniformBuffer, 1000},
            vk::DescriptorPoolSize{       vk::DescriptorType::eStorageBuffer, 1000},
            vk::DescriptorPoolSize{vk::DescriptorType::eUniformBufferDynamic, 1000},
            vk::DescriptorPoolSize{vk::DescriptorType::eStorageBufferDynamic, 1000},
            vk::DescriptorPoolSize{     vk::DescriptorType::eInputAttachment, 1000}
        };

        // Create descriptor pool.
        vk::DescriptorPoolCreateInfo poolInfo{
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = 1000,
            .poolSizeCount = poolSizes.size(),
            .pPoolSizes = poolSizes.data()
        };

        imguiDescriptorPool_ = device.getDevice().createDescriptorPool(poolInfo);

        // Setup Dear ImGui context.
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImPlot::CreateContext();

        // Setup platform/renderer backends.
        ImGui_ImplGlfw_InitForVulkan(window.getWindow(), true);
        vk::Format colorFormat = viewport.getSwapChainImageFormat();
        vk::Format depthFormat = viewport.getDepthFormat();
        ImGui_ImplVulkan_InitInfo initInfo{
            .Instance = *instance.getInstance(),
            .PhysicalDevice = *device.getPhysicalDevice(),
            .Device = *device.getDevice(),
            .QueueFamily = device.getQueue().familyIndex,
            .Queue = *device.getQueue().queue,
            .DescriptorPool = *imguiDescriptorPool_,
            .MinImageCount = 2,
            .ImageCount = 2,
            .UseDynamicRendering = true,
            .PipelineRenderingCreateInfo =
                vk::PipelineRenderingCreateInfo{
                                                .colorAttachmentCount = 1,
                                                .pColorAttachmentFormats = &colorFormat,
                                                .depthAttachmentFormat = depthFormat,
                                                }
        };
        ImGui_ImplVulkan_Init(&initInfo);

        ImGui_ImplVulkan_CreateFontsTexture();
        initialized_ = true;
    }

    ImguiManager::ImguiManager(ImguiManager&& rhs) noexcept :
        initialized_(std::exchange(rhs.initialized_, false)),
        imguiDescriptorPool_(std::exchange(rhs.imguiDescriptorPool_, nullptr)) {}

    ImguiManager& ImguiManager::operator=(ImguiManager&& rhs) noexcept {
        if (this != &rhs) {
            std::swap(imguiDescriptorPool_, rhs.imguiDescriptorPool_);
            std::swap(initialized_, rhs.initialized_);
        }
        return *this;
    }

    ImguiManager::~ImguiManager() {
        if (initialized_) {
            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImPlot::DestroyContext();
            ImGui::DestroyContext();
        }
    }

}
