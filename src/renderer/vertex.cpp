#include "renderer/vertex.h"

namespace yuubi {

const vk::VertexInputBindingDescription Vertex::getBindingDescription() {
    vk::VertexInputBindingDescription bindingDescription{
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = vk::VertexInputRate::eVertex};

    return bindingDescription;
}
const std::array<vk::VertexInputAttributeDescription, 4>
Vertex::getAttributeDescriptions() {
    std::array<vk::VertexInputAttributeDescription, 4> attributeDescriptions;
    attributeDescriptions[0] = {.location = 0,
                                .binding = 0,
                                .format = vk::Format::eR32G32B32Sfloat,
                                .offset = offsetof(Vertex, position)};

    attributeDescriptions[1] = {.location = 1,
                                .binding = 0,
                                .format = vk::Format::eR32G32B32Sfloat,
                                .offset = offsetof(Vertex, normal)};

    attributeDescriptions[2] = {.location = 2,
                                .binding = 0,
                                .format = vk::Format::eR32G32B32A32Sfloat,
                                .offset = offsetof(Vertex, color)};
    attributeDescriptions[3] = {.location = 3,
                                .binding = 0,
                                .format = vk::Format::eR32G32Sfloat,
                                .offset = offsetof(Vertex, uv)};

    return attributeDescriptions;
}

}
