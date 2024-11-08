#pragma once

#include "renderer/vulkan_usage.h"
#include "pch.h"
#include "renderer/push_constants.h"

namespace yuubi {

class Device;
class DrawContext;
class Image;
class Buffer;

struct RenderAttachment {
    vk::Image image;
    vk::ImageView imageView;
};

class AOPass : NonCopyable {
public:
    struct CreateInfo {
        std::shared_ptr<Device> device;
        std::span<vk::DescriptorSetLayout> descriptorSetLayouts;
        std::span<vk::PushConstantRange> pushConstantRanges;
        std::span<vk::Format> colorAttachmentFormats;
        vk::Format depthFormat;
    };

    struct RenderInfo {
        const vk::raii::CommandBuffer& commandBuffer;
        const DrawContext& context;
        vk::Extent2D viewportExtent;
        std::span<vk::DescriptorSet> descriptorSets;
        const Buffer& sceneDataBuffer;
        RenderAttachment color;
        RenderAttachment normal;
        RenderAttachment depth;
    };

    AOPass() = default;
    explicit AOPass(const CreateInfo& createInfo);
    AOPass(AOPass&&) = default;
    AOPass& operator=(AOPass&& rhs) noexcept;

    void render(const RenderInfo& renderInfo);

private:
    vk::raii::PipelineLayout pipelineLayout_ = nullptr;
    vk::raii::Pipeline opaquePipeline_ = nullptr;
    vk::raii::Pipeline transparentPipeline_ = nullptr;
};

}
