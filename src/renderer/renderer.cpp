#include "renderer/renderer.h"

#include <GLFW/glfw3.h>

namespace yuubi {

Renderer::Renderer(const Window& window) : window_(window) {
    instance_ = Instance{context_};

    VkSurfaceKHR tmp;
    if (glfwCreateWindowSurface(static_cast<VkInstance>(*instance_.getInstance()),
                                window_.getWindow(), nullptr,
                                &tmp) != VK_SUCCESS) {
        UB_ERROR("Unable to create window surface!");
    }

    surface_ = vk::raii::SurfaceKHR{instance_, tmp};

    device_ = Device{instance_, surface_};
}

Renderer::~Renderer() {}

}
