#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace yuubi {

// TODO: decouple camera class from renderer
class Camera {
public:
    Camera(glm::vec3 position, glm::vec3 velocity, float pitch, float yaw, float aspectRatio = 800.0f / 600.0f);
    [[nodiscard]] glm::mat4 getViewMatrix() const;
    [[nodiscard]] glm::mat4 getProjectionMatrix() const;
    [[nodiscard]] glm::mat4 getViewProjectionMatrix() const;
    [[nodiscard]] glm::mat4 getRotationMatrix() const;
    [[nodiscard]] glm::vec3 getPosition() const { return position_; };
    void updatePosition(float deltaTime);

    glm::vec3 velocity;
    float pitch = 0.0f;
    float yaw = 0.0f;
    // TODO: look into why this is more precise
    float near = 10000.f;
    float far = .1f;
private:
    glm::vec3 position_;

    float aspectRatio_ = 800.0f / 600.0f;
    float fov_ = glm::radians(90.0f);
};

}
