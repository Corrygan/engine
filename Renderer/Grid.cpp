#include "Grid.h"
#include "glad/gl.h"
#include <vector>
#include <cmath>

namespace {
    struct GridVertex {
        float position[3];
        float color[3];
    };
}

Grid::Grid() {
    std::vector<GridVertex> vertices;

    constexpr float kSize = 10.0f;
    constexpr float kStep = 1.0f;

    constexpr float kGridColor[3] = { 0.30f, 0.30f, 0.33f };
    constexpr float kXAxisColor[3] = { 0.80f, 0.25f, 0.25f };
    constexpr float kZAxisColor[3] = { 0.25f, 0.45f, 0.85f };

    for (float i = -kSize; i <= kSize + 0.001f; i += kStep) {
        bool isOriginLine = std::abs(i) < 0.001f;

        const float* colorZ = isOriginLine ? kZAxisColor : kGridColor;
        vertices.push_back({ { i, 0.0f, -kSize }, { colorZ[0], colorZ[1], colorZ[2] } });
        vertices.push_back({ { i, 0.0f,  kSize }, { colorZ[0], colorZ[1], colorZ[2] } });

        const float* colorX = isOriginLine ? kXAxisColor : kGridColor;
        vertices.push_back({ { -kSize, 0.0f, i }, { colorX[0], colorX[1], colorX[2] } });
        vertices.push_back({ {  kSize, 0.0f, i }, { colorX[0], colorX[1], colorX[2] } });
    }

    m_vertexCount = static_cast<int>(vertices.size());

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GridVertex), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GridVertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GridVertex), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

Grid::~Grid() {
    glDeleteBuffers(1, &m_vbo);
    glDeleteVertexArrays(1, &m_vao);
}

void Grid::Draw() const {
    glBindVertexArray(m_vao);
    glDrawArrays(GL_LINES, 0, m_vertexCount);
    glBindVertexArray(0);
}
