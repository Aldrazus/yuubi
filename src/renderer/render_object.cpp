#include "renderer/render_object.h"

#include "renderer/loaded_gltf.h"

namespace yuubi {

void Node::draw(const glm::mat4& topMatrix, DrawContext& context)
{
    for (auto& child : children_) {
        child->draw(topMatrix, context);
    }
}
void Node::refreshTransform(const glm::mat4& parentMatrix)
{
    worldTransform_ = parentMatrix * localTransform_;
    for (auto& child : children_) {
        child->refreshTransform(worldTransform_);
    }
}

void MeshNode::draw(const glm::mat4& topMatrix, DrawContext& context)
{
    glm::mat4 nodeMatrix = topMatrix * worldTransform_;

    for (auto& surface : mesh_->surfaces()) {
        context.opaqueSurfaces.emplace_back(
            surface.count,
            surface.startIndex,
            mesh_->vertexBuffer(),
            mesh_->indexBuffer(),
            0, // TODO: add material id
            nodeMatrix
        );
    }

    Node::draw(topMatrix, context);
}

}
