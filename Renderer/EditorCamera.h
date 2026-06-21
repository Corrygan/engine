#pragma once
#include <glm/glm.hpp>

// Камера сцены в редакторе — аналог Scene View камеры в Unity. Это инструмент
// навигации для удобства разработки, а не GameObject и не часть симуляции.
class EditorCamera {
public:
    void OrbitDrag(float deltaX, float deltaY);
    void Pan(float deltaX, float deltaY);
    void Zoom(float scrollDelta);

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix(float aspectRatio) const;

private:
    glm::vec3 m_target = glm::vec3(0.0f, 0.0f, 0.0f);
    float m_yaw = -45.0f;    // градусы, вокруг Y
    float m_pitch = 30.0f;   // градусы, вверх/вниз
    float m_distance = 10.0f;
    float m_fov = 45.0f;
};
