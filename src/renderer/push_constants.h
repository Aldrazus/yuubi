#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "renderer/vulkan_usage.h"

namespace yuubi {

struct PushConstants {
    glm::mat4 transform;
    vk::DeviceAddress sceneDataBuffer;
    vk::DeviceAddress vertexBuffer;
    uint32_t materialId;
};

}
