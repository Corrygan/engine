#include "EditorCamera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

void EditorCamera::OrbitDrag(float deltaX, float deltaY) {
    constexpr float kSensitivity = 0.3f;
    m_yaw -= deltaX * kSensitivity;
    m_pitch -= deltaY * kSensitivity;
    m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);
}

void EditorCamera::Pan(float deltaX, float deltaY) {
    float yawRad = glm::radians(m_yaw);
    float pitchRad = glm::radians(m_pitch);

    glm::vec3 offset;
    offset.x = std::cos(pitchRad) * std::sin(yawRad);
    offset.y = std::sin(pitchRad);
    offset.z = std::cos(pitchRad) * std::cos(yawRad);

    glm::vec3 forward = -glm::normalize(offset);
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 up = glm::cross(right, forward);

    const float scale = m_distance * 0.0015f;
    m_target += -right * deltaX * scale + up * deltaY * scale;
}

void EditorCamera::Zoom(float scrollDelta) {
    constexpr float kZoomSpeed = 0.5f;
    m_distance -= scrollDelta * kZoomSpeed;
    m_distance = std::clamp(m_distance, 1.0f, 100.0f);
}

glm::mat4 EditorCamera::GetViewMatrix() const {
    float yawRad = glm::radians(m_yaw);
    float pitchRad = glm::radians(m_pitch);

    glm::vec3 offset;
    offset.x = m_distance * std::cos(pitchRad) * std::sin(yawRad);
    offset.y = m_distance * std::sin(pitchRad);
    offset.z = m_distance * std::cos(pitchRad) * std::cos(yawRad);

    glm::vec3 eye = m_target + offset;
    return glm::lookAt(eye, m_target, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 EditorCamera::GetProjectionMatrix(float aspectRatio) const {
    return glm::perspective(glm::radians(m_fov), aspectRatio, 0.1f, 100.0f);
}
