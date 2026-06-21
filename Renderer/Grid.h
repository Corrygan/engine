#pragma once
#include <cstdint>

// Опорная сетка в плоскости XZ + цветные линии осей X (красная) и Z (синяя),
// проходящие через начало координат — чисто визуальный ориентир, не часть сцены.
class Grid {
public:
    Grid();
    ~Grid();

    void Draw() const;

private:
    uint32_t m_vao = 0;
    uint32_t m_vbo = 0;
    int m_vertexCount = 0;
};
