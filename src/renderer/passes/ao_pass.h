#pragma once

#include "renderer/vulkan_usage.h"
#include "pch.h"
#include "renderer/push_constants.h"
#include "renderer/passes/render_attachment.h"

namespace yuubi {

class Device;
class DrawContext;
class Image;
class Buffer;

class AOPass : NonCopyable {
public:
    struct CreateInfo {
        std::shared_ptr<Device> device;
        std::span<vk::DescriptorSetLayout> descriptorSetLayouts;
        std::span<vk::PushConstantRange> pushConstantRanges;
        std::span<vk::Format> colorAttachmentFormats;
    };

    struct PushConstants {
        glm::mat4 projection;
        float nearPlane;
        float farPlane;
    };

    struct RenderInfo {
        const vk::raii::CommandBuffer& commandBuffer;
        vk::Extent2D viewportExtent;
        std::span<vk::DescriptorSet> descriptorSets;
        RenderAttachment color;
        PushConstants pushConstants;
    };

    AOPass() = default;
    explicit AOPass(const CreateInfo& createInfo);
    AOPass(AOPass&&) = default;
    AOPass& operator=(AOPass&& rhs) noexcept;

    void render(const RenderInfo& renderInfo);

private:
    vk::raii::PipelineLayout pipelineLayout_ = nullptr;
    vk::raii::Pipeline pipeline_ = nullptr;
};

}
