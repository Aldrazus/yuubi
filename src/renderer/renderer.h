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

#include <glm/glm.hpp>

namespace yuubi {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec4 color;
    glm::vec2 uv;

    static const vk::VertexInputBindingDescription getBindingDescription();
    static const std::array<vk::VertexInputAttributeDescription, 4> getAttributeDescriptions();
};

struct PushConstants {
    glm::mat4 mvp;
};

const std::vector<Vertex> vertices = {
    Vertex{
        .position = {0.5f, -0.5f, 0.0f},
        .normal = {0.0f, 0.0f, 0.0f},
        .color = {0.0f, 0.0f, 0.0f, 1.0f},
        .uv = {0.0f, 0.0f}
    },
    Vertex{
        .position = {0.5f, 0.5f, 0.0f},
        .normal = {0.0f, 0.0f, 0.0f},
        .color = {0.5f, 0.5f, 0.5f, 1.0f},
        .uv = {0.0f, 1.0f}
    },
    Vertex{
        .position = {-0.5f, -0.5f, 0.0f},
        .normal = {0.0f, 0.0f, 0.0f},
        .color = {1.0f, 0.0f, 0.0f, 1.0f},
        .uv = {1.0f, 0.0f}
    },
    Vertex{
        .position = {-0.5f, 0.5f, 0.0f},
        .normal = {0.0f, 0.0f, 0.0f},
        .color = {0.0f, 1.0f, 0.0f, 1.0f},
        .uv = {1.0f, 1.0f}
    },
};

const std::vector<uint16_t> indices = {
    0, 1, 2, 2, 1, 3
};

class Renderer {
public:
    Renderer(const Window& window);
    ~Renderer();

    void draw(const Camera& camera);
    void renderScene(std::function<void(Renderer&)> f);

private:
    void createGraphicsPipeline();
    void createVertexBuffer();
    void createIndexBuffer();
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

    Buffer vertexBuffer_;
    Buffer indexBuffer_;

    Texture texture_;

    BindlessSetManager bindlessSetManager_;
};

}
