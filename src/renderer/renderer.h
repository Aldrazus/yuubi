#pragma once

#include "renderer/camera.h"
#include "renderer/descriptor_allocator.h"
#include "renderer/device.h"
#include "renderer/gltf/asset.h"
#include "renderer/imgui_manager.h"
#include "renderer/instance.h"
#include "renderer/render_object.h"
#include "renderer/resources/material_manager.h"
#include "renderer/viewport.h"
#include "renderer/vma/buffer.h"
#include "renderer/resources/texture_manager.h"
#include "window.h"
#include "pch.h"
#include "renderer/vertex.h"
#include "renderer/loaded_gltf.h"

namespace yuubi {

class Renderer {
public:
    explicit Renderer(const Window& window);
    ~Renderer();

    void draw(const Camera& camera, float averageFPS);
    void renderScene(std::function<void(Renderer&)> f);

private:
    void createGraphicsPipeline();
    void initSkybox();
    void createDescriptor();
    void updateScene(const Camera& camera);

    const Window& window_;
    vk::raii::Context context_;
    Instance instance_;
    std::shared_ptr<vk::raii::SurfaceKHR> surface_;
    std::shared_ptr<Device> device_;
    Viewport viewport_;
    ImguiManager imguiManager_;

    vk::raii::PipelineLayout pipelineLayout_ = nullptr;
    vk::raii::Pipeline opaquePipeline_ = nullptr;
    vk::raii::Pipeline transparentPipeline_ = nullptr;

    vk::raii::PipelineLayout skyboxPipelineLayout_ = nullptr;
    vk::raii::Pipeline skyboxPipeline_ = nullptr;
    Buffer skyboxIndexBuffer_;

    DrawContext drawContext_;
    GLTFAsset asset_;
    std::unordered_map<std::string, std::shared_ptr<Node>> loadedNodes_;
    std::shared_ptr<Mesh> mesh_;
    TextureManager textureManager_;

    // Global scene data updated once per frame/draw call.
    Buffer sceneDataBuffer_;

    MaterialManager materialManager_;
};

}
