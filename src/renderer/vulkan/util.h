#pragma once

#include "renderer/vulkan_usage.h"

namespace yuubi {

    void transitionImage(
        const vk::raii::CommandBuffer& commandBuffer, const vk::Image& image, const vk::ImageLayout& currentLayout,
        const vk::ImageLayout& newLayout
    );
}
