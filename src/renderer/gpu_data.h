#pragma once

#include <glm/glm.hpp>
#include "renderer/vulkan_usage.h"

namespace yuubi {

struct SceneData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewproj;
    glm::vec4 cameraPosition;

    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection; // w for sun power
    glm::vec4 sunlightColor;
    vk::DeviceAddress materials;
};

// PERF: pack this struct appropriately
struct MaterialData {
    uint32_t normalTex;
    float scale;

    uint32_t albedoTex;
    uint32_t pad0;
    glm::vec4 albedo;

    uint32_t metallicRoughnessTex;
    float metallicFactor;
    float roughnessFactor;
    
    uint32_t pad1;
};

}
