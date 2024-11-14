#include "renderer/camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace yuubi {

    Camera::Camera(glm::vec3 position, glm::vec3 velocity, float pitch, float yaw, float aspectRatio) :
        velocity(velocity), pitch(pitch), yaw(yaw), position_(position), aspectRatio_(aspectRatio) {}
    glm::mat4 Camera::getViewMatrix() const {
#if 1
        const auto cameraTranslation = glm::translate(glm::mat4(1.0f), position_);
        const auto cameraRotation = getRotationMatrix();
        return glm::inverse(cameraTranslation * cameraRotation);
#else
        const auto quaternion = glm::quat(glm::vec3(glm::radians(pitch), glm::radians(yaw), 0.0f));
        const auto front = glm::rotate(quaternion, glm::vec3(0.0f, 0.0f, -1.0f));
#endif
    }

    glm::mat4 Camera::getProjectionMatrix() const { return glm::perspective(fov_, aspectRatio_, near, far); }

    glm::mat4 Camera::getViewProjectionMatrix() const { return getProjectionMatrix() * getViewMatrix(); }

    glm::mat4 Camera::getRotationMatrix() const {
        const auto pitchRotation = glm::angleAxis(glm::radians(pitch), glm::vec3(1.0f, 0.0f, 0.0f));
        const auto yawRotation = glm::angleAxis(glm::radians(yaw), glm::vec3(0.0f, -1.0f, 0.0f));

        // The following line doesn't work:
        // const auto orientation = glm::normalize(pitchRotation * yawRotation);
        // We must concatenate the two rotation matrices instead of multiplying the
        // two quaternions before casting. This locks the roll (z) axis of rotation.
        return glm::mat4_cast(glm::normalize(yawRotation)) * glm::mat4_cast(glm::normalize(pitchRotation));
    }

    void Camera::updatePosition(float deltaTime) {
        const auto cameraRotation = getRotationMatrix();
        constexpr float moveSpeed = 10.0f;
        position_ += glm::vec3(cameraRotation * glm::vec4(velocity * moveSpeed, 0.0f)) * deltaTime;
    }

}
