#pragma once
#include <glm/glm.hpp>

class EditorCamera {
public:
    void OrbitDrag(float deltaX, float deltaY);
    void Pan(float deltaX, float deltaY);
    void Zoom(float scrollDelta);

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix(float aspectRatio) const;

private:
    glm::vec3 m_target = glm::vec3(0.0f, 0.0f, 0.0f);
    float m_yaw = -45.0f;
    float m_pitch = 30.0f;
    float m_distance = 10.0f;
    float m_fov = 45.0f;
};
