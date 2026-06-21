#pragma once
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
#include "../Scene/GameObject.h"

class Framebuffer;
class Shader;
class CubeMesh;
class Grid;

// Рисует список GameObject'ов сцены в текстуру и возвращает её ID.
// Полностью не знает про ImGui/редактор — просто отдаёт готовую текстуру.
// Камеру (view/projection) ему передают снаружи — сам он камеру не считает.
class SceneRenderer {
public:
    SceneRenderer();
    ~SceneRenderer();

    uint32_t Render(const std::vector<GameObject>& objects, int selectedIndex,
        int width, int height,
        const glm::mat4& view, const glm::mat4& projection);

private:
    Framebuffer* m_framebuffer = nullptr;
    Shader* m_shader = nullptr;        // обычный лит-шейдер для объектов
    Shader* m_lineShader = nullptr;    // безсветовой цветной шейдер для сетки
    Shader* m_outlineShader = nullptr; // контур выделенного объекта
    CubeMesh* m_cube = nullptr;
    Grid* m_grid = nullptr;
};
