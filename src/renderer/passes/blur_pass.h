#pragma once

#include "pch.h"
#include "renderer/vulkan_usage.h"
#include "renderer/passes/render_attachment.h"

namespace yuubi {
    class Device;

    class BlurPass : NonCopyable {
    public:
        struct CreateInfo {
            std::shared_ptr<Device> device;
            vk::Format colorAttachmentFormat;
        };

        struct RenderInfo {
            const vk::raii::CommandBuffer& commandBuffer;
            vk::Extent2D viewportExtent;
            RenderAttachment color;
        };

        BlurPass() = default;
        explicit BlurPass(const CreateInfo& createInfo);
        BlurPass(BlurPass&&) = default;
        BlurPass& operator=(BlurPass&& rhs) noexcept;

        void render(const RenderInfo& renderInfo) const;

    private:
        vk::raii::PipelineLayout pipelineLayout_ = nullptr;
        vk::raii::Pipeline pipeline_ = nullptr;
    };
}
