#include "Mesh.h"
#include "glad/gl.h"
#include <array>
#include <cmath>

namespace {
    constexpr float kPi = 3.14159265358979323846f;

    constexpr float kCubeVertices[] = {
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,

        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,

         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

         -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
          0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
          0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
          0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
         -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
         -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,

         -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
          0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
          0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
          0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
         -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
         -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
    };
}

PrimitiveMesh::~PrimitiveMesh() {
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
}

void PrimitiveMesh::Upload(const std::vector<float>& vertices) {
    m_vertexCount = static_cast<int>(vertices.size() / 6);

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void PrimitiveMesh::Draw() const {
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, m_vertexCount);
    glBindVertexArray(0);
}

CubeMesh::CubeMesh() {
    Upload(std::vector<float>(std::begin(kCubeVertices), std::end(kCubeVertices)));
}

SphereMesh::SphereMesh(int latSegments, int lonSegments) {
    constexpr float kRadius = 0.5f;

    std::vector<float> vertices;
    vertices.reserve(static_cast<size_t>(latSegments) * lonSegments * 6 * 6);

    auto direction = [](float theta, float phi) {
        float sinTheta = std::sin(theta);
        return std::array<float, 3>{
            sinTheta* std::cos(phi),
                std::cos(theta),
                sinTheta* std::sin(phi)
        };
        };

    auto pushVertex = [&vertices, kRadius](const std::array<float, 3>& dir) {
        vertices.push_back(dir[0] * kRadius);
        vertices.push_back(dir[1] * kRadius);
        vertices.push_back(dir[2] * kRadius);
        vertices.push_back(dir[0]);
        vertices.push_back(dir[1]);
        vertices.push_back(dir[2]);
        };

    for (int lat = 0; lat < latSegments; ++lat) {
        float theta1 = lat * kPi / latSegments;
        float theta2 = (lat + 1) * kPi / latSegments;

        for (int lon = 0; lon < lonSegments; ++lon) {
            float phi1 = lon * 2.0f * kPi / lonSegments;
            float phi2 = (lon + 1) * 2.0f * kPi / lonSegments;

            auto p1 = direction(theta1, phi1);
            auto p2 = direction(theta2, phi1);
            auto p3 = direction(theta2, phi2);
            auto p4 = direction(theta1, phi2);

            pushVertex(p1); pushVertex(p2); pushVertex(p3);
            pushVertex(p1); pushVertex(p3); pushVertex(p4);
        }
    }

    Upload(vertices);
}

PlaneMesh::PlaneMesh() {
    const std::vector<float> vertices = {
        -0.5f, 0.0f, -0.5f,  0.0f, 1.0f, 0.0f,
         0.5f, 0.0f, -0.5f,  0.0f, 1.0f, 0.0f,
         0.5f, 0.0f,  0.5f,  0.0f, 1.0f, 0.0f,

         0.5f, 0.0f,  0.5f,  0.0f, 1.0f, 0.0f,
        -0.5f, 0.0f,  0.5f,  0.0f, 1.0f, 0.0f,
        -0.5f, 0.0f, -0.5f,  0.0f, 1.0f, 0.0f,
    };
    Upload(vertices);
}