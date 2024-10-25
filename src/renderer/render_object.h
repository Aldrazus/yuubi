#pragma once

#include "pch.h"
#include <glm/glm.hpp>

namespace yuubi {

class Buffer;
struct RenderObject {
    uint32_t indexCount;
    uint32_t firstIndex;
    std::shared_ptr<Buffer> vertexBuffer;
    std::shared_ptr<Buffer> indexBuffer;
    uint32_t materialId;
    glm::mat4 transform;
};

struct DrawContext {
    std::vector<RenderObject> opaqueSurfaces;
    std::vector<RenderObject> transparentSurfaces;
};

class Renderable {
public:
    virtual void draw(const glm::mat4& topMatrix, DrawContext& context) = 0;
    virtual ~Renderable() = default;
};

class Node : public Renderable {
public:
    virtual void draw(const glm::mat4& topMatrix, DrawContext& context) override;
    void refreshTransform(const glm::mat4& parentMatrix);

    glm::mat4 localTransform = glm::mat4{ 1.0f};
    glm::mat4 worldTransform = glm::mat4{1.0f};
    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;
};

class Mesh;
class MeshNode : public Node {
public:
    explicit MeshNode(std::shared_ptr<Mesh> mesh) : mesh_(std::move(mesh)) {}
    virtual void draw(const glm::mat4& topMatrix, DrawContext& context) override;
private:
    std::shared_ptr<Mesh> mesh_;
};

}
