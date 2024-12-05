#pragma once

#include "pch.h"
#include "renderer/vulkan_usage.h"
#include "renderer/passes/render_attachment.h"
#include "renderer/vma/buffer.h"

namespace yuubi {

    class Device;

    class PrefilterPass : NonCopyable {
    public:
        struct CreateInfo {
            std::shared_ptr<Device> device;
            std::span<vk::DescriptorSetLayout> descriptorSetLayouts;
            vk::Format colorAttachmentFormat;
        };

        struct PushConstants {
            vk::DeviceAddress viewProjectionMatrices;
            float roughness;
        };

        struct RenderInfo {
            const vk::raii::CommandBuffer& commandBuffer;
            vk::Extent2D viewportExtent;
            std::span<vk::DescriptorSet> descriptorSets;
            RenderAttachment color;
            float roughness;
        };

        PrefilterPass() = default;
        explicit PrefilterPass(const CreateInfo& createInfo);
        PrefilterPass(PrefilterPass&&) = default;
        PrefilterPass& operator=(PrefilterPass&& rhs) noexcept;

        void render(const RenderInfo& renderInfo) const;

    private:
        vk::raii::PipelineLayout pipelineLayout_ = nullptr;
        vk::raii::Pipeline pipeline_ = nullptr;
        Buffer viewProjectionMatricesBuffer_;
    };

}
