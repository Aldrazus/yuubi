#pragma once

#include "renderer/device.h"
#include "renderer/instance.h"
#include "window.h"

namespace yuubi {

class Renderer {
public:
    Renderer(const Window& window);
    ~Renderer();

private:
    const Window& window_;
    vk::raii::Context context_;
    Instance instance_;
    vk::raii::SurfaceKHR surface_ = nullptr;
    Device device_;
};

}
