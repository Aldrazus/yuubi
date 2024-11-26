#pragma once

#include "renderer/vulkan_usage.h"
#include "pch.h"
#include "renderer/passes/render_attachment.h"
#include <glm/glm.hpp>
#include <renderer/vma/buffer.h>

namespace yuubi {

    class Device;

    // Renders equirectangular map to cubemap
    class CubemapPass : NonCopyable {
    public:
        struct CreateInfo {
            std::shared_ptr<Device> device;
            std::span<vk::DescriptorSetLayout> descriptorSetLayouts;
            vk::Format colorAttachmentFormat;
        };

        struct PushConstants {
            vk::DeviceAddress viewProjectionMatrices;
        };

        struct RenderInfo {
            const vk::raii::CommandBuffer& commandBuffer;
            vk::Extent2D viewportExtent;
            std::span<vk::DescriptorSet> descriptorSets;
            RenderAttachment color;
        };

        CubemapPass() = default;
        explicit CubemapPass(const CreateInfo& createInfo);
        CubemapPass(CubemapPass&&) = default;
        CubemapPass& operator=(CubemapPass&& rhs) noexcept;

        void render(const RenderInfo& renderInfo) const;

    private:
        vk::raii::PipelineLayout pipelineLayout_ = nullptr;
        vk::raii::Pipeline pipeline_ = nullptr;
        Buffer viewProjectionMatricesBuffer_;
    };

} // yuubi
