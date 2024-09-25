#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "renderer/vulkan_usage.h"

namespace yuubi {

struct PushConstants {
    glm::mat4 mvp;
    vk::DeviceAddress sceneDataBuffer;
    vk::DeviceAddress vertexBuffer;
};

}
