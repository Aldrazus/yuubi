#pragma once

#include <glm/glm.hpp>
#include "renderer/vulkan_usage.h"

namespace yuubi {

struct SceneData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewproj;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection; // w for sun power
    glm::vec4 sunlightColor;
    vk::DeviceAddress materials;
};

struct MaterialData {
    glm::vec4 albedo;
    uint32_t albedoTex;

    float metallicFactor;
    float roughnessFactor;
    uint32_t metallicRoughnessTex;
};

}
