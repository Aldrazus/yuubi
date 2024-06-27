#pragma once

#include "renderer/camera.h"
#include "renderer/device.h"
#include "renderer/instance.h"
#include "renderer/viewport.h"
#include "renderer/vma/buffer.h"
#include "window.h"
#include "pch.h"

#include <glm/glm.hpp>

namespace yuubi {

struct Vertex {
    glm::vec3 pos;
    glm::vec4 color;
};

struct PushConstants {
    glm::mat4 mvp;
    vk::DeviceAddress vertexBuffer;
};

const std::vector<Vertex> vertices = {
    {{0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.5f, 0.5f, 0.5f, 1.0f}},
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
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
    // TODO: maybe move to utils
    void transitionImage(const vk::raii::CommandBuffer& commandBuffer, const vk::Image& image, const vk::ImageLayout& currentLayout, const vk::ImageLayout& newLayout);

    void createGraphicsPipeline();
    void createVertexBuffer();
    void createIndexBuffer();
    void createDescriptor();
    void createImmediateCommandBuffer();
    void submitImmediateCommands(std::function<void(const vk::raii::CommandBuffer& commandBuffer)>&& function);

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

    vk::raii::CommandPool immediateCommandPool_ = nullptr;
    vk::raii::CommandBuffer immediateCommandBuffer_ = nullptr;
    vk::raii::Fence immediateCommandFence_ = nullptr;
};

}
