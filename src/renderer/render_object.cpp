#include "renderer/render_object.h"

#include "renderer/loaded_gltf.h"

namespace yuubi {

void Node::draw(const glm::mat4& topMatrix, DrawContext& context)
{
    for (auto& child : children) {
        child->draw(topMatrix, context);
    }
}
void Node::refreshTransform(const glm::mat4& parentMatrix)
{
    worldTransform = parentMatrix * localTransform;
    for (auto& child : children) {
        child->refreshTransform(worldTransform);
    }
}

void MeshNode::draw(const glm::mat4& topMatrix, DrawContext& context)
{
    glm::mat4 nodeMatrix = topMatrix * worldTransform;

    for (auto& surface : mesh_->surfaces()) {
        context.opaqueSurfaces.emplace_back(
            surface.count,
            surface.startIndex,
            mesh_->vertexBuffer(),
            mesh_->indexBuffer(),
            surface.materialIndex,
            nodeMatrix
        );
    }

    Node::draw(topMatrix, context);
}

}
