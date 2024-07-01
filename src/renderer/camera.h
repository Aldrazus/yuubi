#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace yuubi {

class Camera {
public:
    Camera(glm::vec3 position, glm::vec3 velocity, float pitch, float yaw);
    glm::mat4 getViewMatrix() const;
    glm::mat4 getViewProjectionMatrix() const;
    glm::mat4 getRotationMatrix() const;
    void updatePosition(float deltaTime);

    glm::vec3 velocity;
    float pitch = 0.0f;
    float yaw = 0.0f;
private:
    glm::vec3 position_;
    float near_ = 0.1f;
    float far_ = 75.0f;
    float aspectRatio_ = 800.0f / 600.0f;
    float fov_ = glm::radians(90.0f);
};

}
