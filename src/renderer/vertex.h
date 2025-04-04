#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>
#include "pch.h"

namespace yuubi {

    struct Vertex {
        glm::vec3 position;
        float uv_x;
        glm::vec3 normal;
        float uv_y;
        glm::vec4 color;
        glm::vec4 tangent;
    };

}
