#pragma once
#include <cstdint>

// Заглушка-геометрия: единичный куб с нормалями (для базового освещения).
// Пока в движке нет импорта мешей — каждый GameObject рисуется как такой куб.
class CubeMesh {
public:
    CubeMesh();
    ~CubeMesh();

    void Draw() const;

private:
    uint32_t m_vao = 0;
    uint32_t m_vbo = 0;
};
