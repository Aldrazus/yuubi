#pragma once

#include "renderer/device.h"
#include "renderer/instance.h"
#include "renderer/viewport.h"
#include "window.h"
#include "pch.h"

namespace yuubi {

class Renderer {
public:
    Renderer(const Window& window);
    ~Renderer();

    void draw();

private:
    // TODO: maybe move to utils
    void transitionImage(const vk::raii::CommandBuffer& commandBuffer, const vk::Image& image, const vk::ImageLayout& currentLayout, const vk::ImageLayout& newLayout);

    const Window& window_;
    vk::raii::Context context_;
    Instance instance_;
    std::shared_ptr<vk::raii::SurfaceKHR> surface_;
    std::shared_ptr<Device> device_;
    Viewport viewport_;
};

}
