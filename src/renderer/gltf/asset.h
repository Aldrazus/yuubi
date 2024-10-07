#pragma once

#include "renderer/render_object.h"
#include "renderer/vulkan_usage.h"
#include "pch.h"

namespace yuubi {

class Image;
class Device;
class Node;
class Mesh;
class GLTFAsset : NonCopyable, public Renderable {
public:
    GLTFAsset() = default;
    GLTFAsset(Device& device, const std::filesystem::path& filePath);

    // TODO: add move constructor/assignment operator

    virtual void draw(const glm::mat4& topMatrix, DrawContext& context) override;
private:
    // GLTF resources.
    std::unordered_map<std::string, std::shared_ptr<Mesh>> meshes_;
    std::unordered_map<std::string, std::shared_ptr<Node>> nodes_;

    std::vector<std::shared_ptr<Node>> topNodes_;
};

}
