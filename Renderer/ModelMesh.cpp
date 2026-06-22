#include "ModelMesh.h"
#include "../Assets/EmdlFormat.h"
#include "glad/gl.h"

#include <fstream>
#include <vector>

ModelMesh::~ModelMesh() {
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_ebo) glDeleteBuffers(1, &m_ebo);
}

ModelMesh* ModelMesh::LoadFromEmdl(const std::string& emdlPath) {
    std::ifstream file(emdlPath, std::ios::binary);
    if (!file.is_open()) return nullptr;

    EmdlHeader header{};
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (header.magic != kEmdlMagic || header.version != kEmdlVersion) return nullptr;

    std::vector<EmdlVertex> vertices(header.vertexCount);
    std::vector<uint32_t>   indices(header.indexCount);
    file.read(reinterpret_cast<char*>(vertices.data()), vertices.size() * sizeof(EmdlVertex));
    file.read(reinterpret_cast<char*>(indices.data()),  indices.size()  * sizeof(uint32_t));

    auto* mesh = new ModelMesh();
    mesh->m_indexCount = static_cast<int>(header.indexCount);
    mesh->m_aabbMin = glm::vec3(header.aabbMin[0], header.aabbMin[1], header.aabbMin[2]);
    mesh->m_aabbMax = glm::vec3(header.aabbMax[0], header.aabbMax[1], header.aabbMax[2]);

    glGenVertexArrays(1, &mesh->m_vao);
    glGenBuffers(1, &mesh->m_vbo);
    glGenBuffers(1, &mesh->m_ebo);

    glBindVertexArray(mesh->m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, mesh->m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(EmdlVertex)),
        vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(indices.size() * sizeof(uint32_t)),
        indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(EmdlVertex),
        reinterpret_cast<void*>(offsetof(EmdlVertex, pos)));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(EmdlVertex),
        reinterpret_cast<void*>(offsetof(EmdlVertex, normal)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(EmdlVertex),
        reinterpret_cast<void*>(offsetof(EmdlVertex, uv)));
    glEnableVertexAttribArray(2);

    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(EmdlVertex),
        reinterpret_cast<void*>(offsetof(EmdlVertex, tangent)));
    glEnableVertexAttribArray(3);

    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(EmdlVertex),
        reinterpret_cast<void*>(offsetof(EmdlVertex, bitangent)));
    glEnableVertexAttribArray(4);

    glBindVertexArray(0);

    return mesh;
}

bool ModelMesh::ReadAabb(const std::string& emdlPath, glm::vec3& outMin, glm::vec3& outMax) {
    std::ifstream file(emdlPath, std::ios::binary);
    if (!file.is_open()) return false;
    EmdlHeader header{};
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (header.magic != kEmdlMagic || header.version != kEmdlVersion) return false;
    outMin = glm::vec3(header.aabbMin[0], header.aabbMin[1], header.aabbMin[2]);
    outMax = glm::vec3(header.aabbMax[0], header.aabbMax[1], header.aabbMax[2]);
    return true;
}

void ModelMesh::Draw() const {
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}
