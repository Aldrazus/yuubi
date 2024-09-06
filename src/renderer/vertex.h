#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>
#include "pch.h"

namespace yuubi {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec4 color;
    glm::vec2 uv;

    static const vk::VertexInputBindingDescription getBindingDescription();
    static const std::array<vk::VertexInputAttributeDescription, 4> getAttributeDescriptions();
};

}
