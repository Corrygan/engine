#pragma once
#include <string>
#include <cstdint>
#include <glm/glm.hpp>

class ModelMesh {
public:
    ~ModelMesh();

    static ModelMesh* LoadFromEmdl(const std::string& emdlPath);
    static bool ReadAabb(const std::string& emdlPath, glm::vec3& outMin, glm::vec3& outMax);

    void Draw() const;
    glm::vec3 GetAabbMin() const { return m_aabbMin; }
    glm::vec3 GetAabbMax() const { return m_aabbMax; }

private:
    ModelMesh() = default;

    uint32_t  m_vao        = 0;
    uint32_t  m_vbo        = 0;
    uint32_t  m_ebo        = 0;
    int       m_indexCount = 0;
    glm::vec3 m_aabbMin{};
    glm::vec3 m_aabbMax{};
};
