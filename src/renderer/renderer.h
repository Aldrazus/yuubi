#pragma once

#include "renderer/camera.h"
#include "renderer/passes/depth_pass.h"
#include "renderer/device.h"
#include "renderer/gltf/asset.h"
#include "renderer/imgui_manager.h"
#include "renderer/instance.h"
#include "renderer/passes/lighting_pass.h"
#include "renderer/render_object.h"
#include "renderer/resources/material_manager.h"
#include "renderer/viewport.h"
#include "renderer/vma/buffer.h"
#include "renderer/resources/texture_manager.h"
#include "window.h"
#include "pch.h"
#include "renderer/passes/brdflut_pass.h"
#include "passes/cubemap_pass.h"
#include "renderer/vertex.h"
#include "renderer/loaded_gltf.h"
#include "renderer/passes/composite_pass.h"
#include "renderer/passes/ao_pass.h"
#include "renderer/passes/skybox_pass.h"
#include "renderer/passes/irradiance_pass.h"
#include "renderer/passes/prefilter_pass.h"

struct AppState;

namespace yuubi {
    class Renderer {
    public:
        explicit Renderer(const Window& window);
        ~Renderer();

        void draw(const Camera& camera, AppState state);

    private:
        void initSkybox();
        void initCompositePassResources();
        void initAOPassResources();
        void initCubemapPassResources();
        void updateScene(const Camera& camera);
        void initIrradianceMapPassResources();
        void generateEnvironmentMap() const;
        void generateIrradianceMap() const;
        void initPrefilterMapPassResources();
        void generatePrefilterMap() const;
        void initBRDFLUTPassResources();
        void generateBRDFLUT() const;
        void initTextureManager();

        const Window& window_;
        vk::raii::Context context_;
        Instance instance_;
        std::shared_ptr<vk::raii::SurfaceKHR> surface_;
        std::shared_ptr<Device> device_;
        std::shared_ptr<Viewport> viewport_;
        ImguiManager imguiManager_;

        // Skybox.
        vk::raii::DescriptorSetLayout skyboxDescriptorSetLayout_ = nullptr;
        vk::raii::DescriptorPool skyboxDescriptorPool_ = nullptr;
        vk::raii::DescriptorSet skyboxDescriptorSet_ = nullptr;
        SkyboxPass skyboxPass_;

        // Composite.
        vk::raii::DescriptorSetLayout compositeDescriptorSetLayout_ = nullptr;
        vk::raii::DescriptorPool compositeDescriptorPool_ = nullptr;
        vk::raii::DescriptorSet compositeDescriptorSet_ = nullptr;
        CompositePass compositePass_;


        // Ambient occlusion.
        vk::raii::DescriptorSetLayout aoDescriptorSetLayout_ = nullptr;
        vk::raii::DescriptorPool aoDescriptorPool_ = nullptr;
        vk::raii::DescriptorSet aoDescriptorSet_ = nullptr;
        Image aoNoiseImage_;
        vk::raii::ImageView aoNoiseImageView_ = nullptr;
        vk::raii::Sampler aoNoiseSampler_ = nullptr;
        AOPass aoPass_;


        DrawContext drawContext_;
        GLTFAsset asset_;
        std::unordered_map<std::string, std::shared_ptr<Node>> loadedNodes_;
        std::shared_ptr<Mesh> mesh_;

        // Global scene data updated once per frame/draw call.
        Buffer sceneDataBuffer_;

        MaterialManager materialManager_;

        DepthPass depthPass_;

        // Lighting
        vk::raii::DescriptorSetLayout textureDescriptorSetLayout_ = nullptr;
        vk::raii::DescriptorSetLayout iblDescriptorSetLayout_ = nullptr;
        vk::raii::DescriptorPool lightingDescriptorPool_ = nullptr;
        vk::raii::DescriptorSet iblDescriptorSet_ = nullptr;
        std::shared_ptr<vk::raii::DescriptorSet> textureDescriptorSet_;
        LightingPass lightingPass_;
        TextureManager textureManager_;

        // Cubemap.
        CubemapPass cubemapPass_;
        Image equirectangularMapImage_;
        vk::raii::ImageView equirectangularMapImageView_ = nullptr;
        vk::raii::Sampler equirectangularMapSampler_ = nullptr;
        Image cubemapImage_;
        vk::raii::ImageView cubemapImageView_ = nullptr;
        vk::raii::Sampler cubemapSampler_ = nullptr;
        vk::raii::DescriptorSetLayout cubemapDescriptorSetLayout_ = nullptr;
        vk::raii::DescriptorPool cubemapDescriptorPool_ = nullptr;
        vk::raii::DescriptorSet cubemapDescriptorSet_ = nullptr;

        // Irradiance map.
        IrradiancePass irradiancePass_;
        Image irradianceMapImage_;
        vk::raii::ImageView irradianceMapImageView_ = nullptr;
        vk::raii::Sampler irradianceMapSampler_ = nullptr;
        vk::raii::DescriptorSetLayout irradianceMapDescriptorSetLayout_ = nullptr;
        vk::raii::DescriptorPool irradianceMapDescriptorPool_ = nullptr;
        vk::raii::DescriptorSet irradianceMapDescriptorSet_ = nullptr;

        // Prefilter map.
        PrefilterPass prefilterPass_;
        Image prefilterMapImage_;
        vk::raii::ImageView prefilterMapImageView_ = nullptr;
        vk::raii::Sampler prefilterMapSampler_ = nullptr;
        vk::raii::DescriptorSetLayout prefilterMapDescriptorSetLayout_ = nullptr;
        vk::raii::DescriptorPool prefilterMapDescriptorPool_ = nullptr;
        vk::raii::DescriptorSet prefilterMapDescriptorSet_ = nullptr;

        // BRDFLUT map.
        BRDFLUTPass brdflutPass_;
        Image brdfLutMapImage_;
        vk::raii::ImageView brdfLutMapImageView_ = nullptr;
        vk::raii::Sampler brdfLutMapSampler_ = nullptr;
    };

}
