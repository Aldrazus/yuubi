#pragma once

#include "renderer/render_object.h"
#include "renderer/vulkan_usage.h"
#include "pch.h"

namespace yuubi {

    class Image;
    class Device;
    class Node;
    class Mesh;
    class TextureManager;
    class MaterialManager;
    class GLTFAsset final : NonCopyable, public Renderable {
    public:
        GLTFAsset() = default;
        GLTFAsset(
            Device& device, TextureManager& textureManager, MaterialManager& materialManager,
            const std::filesystem::path& filePath
        );

        // TODO: add move constructor/assignment operator

        void draw(const glm::mat4& topMatrix, DrawContext& context) override;

    private:
        // GLTF resources.
        std::unordered_map<std::string, std::shared_ptr<Mesh>> meshes_;
        std::unordered_map<std::string, std::shared_ptr<Node>> nodes_;

        std::vector<std::shared_ptr<Node>> topNodes_;
    };

}
