#pragma once

#include "pch.h"
#include "renderer/render_object.h"
#include "renderer/vulkan_usage.h"
#include <glm/glm.hpp>

namespace yuubi {

class Device;
class Viewport;
class DepthPass : NonCopyable {
public:
    DepthPass() = default;

    DepthPass(
        std::shared_ptr<Device> device, std::shared_ptr<Viewport> viewport,
        std::span<vk::DescriptorSetLayout> setLayouts
    );

    DepthPass(DepthPass&&) = default;

    DepthPass& operator=(DepthPass&& rhs) noexcept;

    void render(
        const vk::raii::CommandBuffer&,
        const DrawContext& context,
        const Buffer& sceneDataBuffer,
        const vk::DescriptorSet& descriptorSet
    ) const;

private:
    std::shared_ptr<Device> device_;
    std::shared_ptr<Viewport> viewport_;

    vk::raii::PipelineLayout pipelineLayout_ = nullptr;
    vk::raii::Pipeline pipeline_ = nullptr;
};

}
