#include "EditorCamera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

void EditorCamera::OrbitDrag(float deltaX, float deltaY) {
    constexpr float kSensitivity = 0.3f;
    // Инвертировано: тащим мышку влево -> сцена крутится вправо (и наоборот) —
    // так это ощущается "правильно" при прямом управлении мышкой.
    m_yaw -= deltaX * kSensitivity;
    m_pitch -= deltaY * kSensitivity;
    // Не даём камере "перевернуться" через полюс — иначе на полюсе она
    // резко дёргается в другую сторону.
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

    // Скорость панорамирования зависит от текущей дистанции — иначе вблизи
    // объекта панорама будет слишком резкой, а издали слишком медленной.
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

    // Сферические координаты -> декартовы: позиция камеры на сфере вокруг target.
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
