#pragma once

#include "renderer/vulkan_usage.h"
#include "pch.h"
#include "renderer/passes/render_attachment.h"

#include <glm/glm.hpp>

namespace yuubi {
    class Device;
    class Image;
    class Buffer;

    // Renders a cubemap as a skybox in the scene.
    class SkyboxPass : NonCopyable {
    public:
        struct CreateInfo {
            std::shared_ptr<Device> device;
            std::span<vk::DescriptorSetLayout> descriptorSetLayouts;
            std::span<vk::Format> colorAttachmentFormats;
            vk::Format depthAttachmentFormat;
        };

        struct PushConstants {
            glm::mat4 viewProjection;
        };

        struct RenderInfo {
            const vk::raii::CommandBuffer& commandBuffer;
            vk::Extent2D viewportExtent;
            std::span<vk::DescriptorSet> descriptorSets;
            RenderAttachment color;
            RenderAttachment depth;
            PushConstants pushConstants;
        };

        SkyboxPass() = default;
        explicit SkyboxPass(const CreateInfo& createInfo);
        SkyboxPass(SkyboxPass&&) = default;
        SkyboxPass& operator=(SkyboxPass&& rhs) noexcept;

        void render(const RenderInfo& renderInfo) const;

    private:
        vk::raii::PipelineLayout pipelineLayout_ = nullptr;
        vk::raii::Pipeline pipeline_ = nullptr;
    };

} // yuubi
