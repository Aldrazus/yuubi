#include "renderer/camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace yuubi {

Camera::Camera(glm::vec3 position, glm::vec3 velocity, float pitch, float yaw)
: velocity(velocity), pitch(pitch), yaw(yaw), position_(position) {
}
glm::mat4 Camera::getViewMatrix() const
{
    #if 1
    const auto cameraTranslation = glm::translate(glm::mat4(1.0f), position_);
    const auto cameraRotation = getRotationMatrix();
    return glm::inverse(cameraTranslation * cameraRotation);
    #else
    const auto quaternion = glm::quat(glm::vec3(glm::radians(pitch), glm::radians(yaw), 0.0f));
    const auto front = glm::rotate(quaternion, glm::vec3(0.0f, 0.0f, -1.0f));
    #endif
}

glm::mat4 Camera::getRotationMatrix() const
{
    const auto pitchRotation = glm::angleAxis(glm::radians(pitch), glm::vec3(1.0f, 0.0f, 0.0f));
    const auto yawRotation = glm::angleAxis(glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));
    // const auto yawRotation = glm::angleAxis(yaw, glm::vec3(0.0f, -1.0f, 0.0f));

    const auto orientation = glm::normalize(pitchRotation * yawRotation);
    
    return glm::mat4_cast(orientation);
}

void Camera::updatePosition(float deltaTime)
{
    const auto cameraRotation = getRotationMatrix();
    position_ += glm::vec3(cameraRotation * glm::vec4(velocity * 0.5f, 0.0f)) * deltaTime;
}


}
