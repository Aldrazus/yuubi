#pragma once

#include "pch.h"
#include "renderer/vulkan_usage.h"
#include "renderer/passes/render_attachment.h"
#include "renderer/vma/buffer.h"

namespace yuubi {

    class Device;

    class IrradiancePass : NonCopyable {
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

        IrradiancePass() = default;
        explicit IrradiancePass(const CreateInfo& createInfo);
        IrradiancePass(IrradiancePass&&) = default;
        IrradiancePass& operator=(IrradiancePass&& rhs) noexcept;

        void render(const RenderInfo& renderInfo) const;

    private:
        vk::raii::PipelineLayout pipelineLayout_ = nullptr;
        vk::raii::Pipeline pipeline_ = nullptr;
        Buffer viewProjectionMatricesBuffer_;
    };

} // yuubi
