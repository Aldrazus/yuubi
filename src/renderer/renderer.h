#pragma once

#include "renderer/camera.h"
#include "renderer/descriptor_allocator.h"
#include "renderer/device.h"
#include "renderer/instance.h"
#include "renderer/viewport.h"
#include "renderer/vma/buffer.h"
#include "renderer/bindless_set_manager.h"
#include "renderer/resources/texture.h"
#include "window.h"
#include "pch.h"
#include "renderer/vertex.h"
#include "renderer/loaded_gltf.h"

#include <glm/glm.hpp>

namespace yuubi {

struct PushConstants {
    glm::mat4 mvp;
    vk::DeviceAddress vertexBuffer;
};

class Renderer {
public:
    explicit Renderer(const Window& window);
    ~Renderer();

    void draw(const Camera& camera, float averageFPS);
    void renderScene(std::function<void(Renderer&)> f);

private:
    void createGraphicsPipeline();
    void createDescriptor();
    void initImGui();

    const Window& window_;
    vk::raii::Context context_;
    Instance instance_;
    std::shared_ptr<vk::raii::SurfaceKHR> surface_;
    std::shared_ptr<Device> device_;
    Viewport viewport_;

    vk::raii::PipelineLayout pipelineLayout_ = nullptr;
    vk::raii::Pipeline graphicsPipeline_ = nullptr;

    std::shared_ptr<Mesh> mesh_;

    Texture texture_;

    BindlessSetManager bindlessSetManager_;

    vk::raii::DescriptorPool imguiDescriptorPool_ = nullptr;
};

}
