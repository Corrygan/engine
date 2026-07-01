#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include <utility>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// ── Skeleton / animation CPU data (loaded from a skinned .emdl) ──────────────
struct ModelBone {
    int       parent = -1;
    glm::mat4 offset{ 1.0f };     // inverse-bind (mesh space -> bone space)
    glm::mat4 localBind{ 1.0f };  // node's default local transform
};

struct AnimChannel {
    int bone = -1;
    std::vector<std::pair<float, glm::vec3>> pos;
    std::vector<std::pair<float, glm::quat>> rot;
    std::vector<std::pair<float, glm::vec3>> scl;
};

struct AnimClip {
    std::string              name;
    float                    duration = 0.0f;   // ticks
    float                    tps      = 25.0f;   // ticks per second
    std::vector<AnimChannel> channels;
};

class ModelMesh {
public:
    ~ModelMesh();

    static ModelMesh* LoadFromEmdl(const std::string& emdlPath);
    static bool ReadAabb(const std::string& emdlPath, glm::vec3& outMin, glm::vec3& outMax);

    void Draw() const;
    glm::vec3 GetAabbMin() const { return m_aabbMin; }
    glm::vec3 GetAabbMax() const { return m_aabbMax; }

    // Skeletal data (empty for static meshes).
    bool                          IsSkinned()     const { return m_skinned; }
    int                           BoneCount()     const { return static_cast<int>(m_bones.size()); }
    const std::vector<ModelBone>& Bones()         const { return m_bones; }
    const std::vector<AnimClip>&  Anims()         const { return m_anims; }
    const glm::mat4&              GlobalInverse() const { return m_globalInverse; }
    const std::vector<glm::mat4>& BindPose()      const { return m_bindPose; }

private:
    ModelMesh() = default;

    uint32_t  m_vao        = 0;
    uint32_t  m_vbo        = 0;
    uint32_t  m_ebo        = 0;
    uint32_t  m_skinVbo    = 0;
    int       m_indexCount = 0;
    glm::vec3 m_aabbMin{};
    glm::vec3 m_aabbMax{};

    bool                   m_skinned = false;
    glm::mat4              m_globalInverse{ 1.0f };
    std::vector<ModelBone> m_bones;
    std::vector<AnimClip>  m_anims;
    std::vector<glm::mat4> m_bindPose;   // precomputed rest-pose skin matrices
};
