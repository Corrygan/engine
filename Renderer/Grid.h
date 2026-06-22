#pragma once
#include <cstdint>

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
