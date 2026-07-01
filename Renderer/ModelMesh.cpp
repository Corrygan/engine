#include "ModelMesh.h"
#include "../Assets/EmdlFormat.h"
#include "glad/gl.h"

#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <vector>

ModelMesh::~ModelMesh() {
    if (m_vao)     glDeleteVertexArrays(1, &m_vao);
    if (m_vbo)     glDeleteBuffers(1, &m_vbo);
    if (m_ebo)     glDeleteBuffers(1, &m_ebo);
    if (m_skinVbo) glDeleteBuffers(1, &m_skinVbo);
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

    // ── Skinned data (optional) ────────────────────────────────────────────
    if (header.boneCount > 0) {
        std::vector<EmdlSkin> skins(header.vertexCount);
        std::vector<EmdlBone> bones(header.boneCount);
        file.read(reinterpret_cast<char*>(skins.data()), skins.size() * sizeof(EmdlSkin));
        file.read(reinterpret_cast<char*>(bones.data()), bones.size() * sizeof(EmdlBone));

        glGenBuffers(1, &mesh->m_skinVbo);
        glBindBuffer(GL_ARRAY_BUFFER, mesh->m_skinVbo);
        glBufferData(GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(skins.size() * sizeof(EmdlSkin)),
            skins.data(), GL_STATIC_DRAW);
        glVertexAttribIPointer(5, 4, GL_INT, sizeof(EmdlSkin),
            reinterpret_cast<void*>(offsetof(EmdlSkin, boneIds)));
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(EmdlSkin),
            reinterpret_cast<void*>(offsetof(EmdlSkin, weights)));
        glEnableVertexAttribArray(6);

        mesh->m_skinned = true;
        mesh->m_globalInverse = glm::make_mat4(header.globalInverse);

        mesh->m_bones.resize(header.boneCount);
        for (uint32_t i = 0; i < header.boneCount; ++i) {
            mesh->m_bones[i].parent    = bones[i].parent;
            mesh->m_bones[i].offset    = glm::make_mat4(bones[i].offset);
            mesh->m_bones[i].localBind = glm::make_mat4(bones[i].localBind);
        }

        // Animation clips (variable-length tail).
        auto readU32 = [&](uint32_t& v) { file.read(reinterpret_cast<char*>(&v), 4); };
        auto readF   = [&](float& v)    { file.read(reinterpret_cast<char*>(&v), 4); };
        mesh->m_anims.resize(header.animCount);
        for (uint32_t a = 0; a < header.animCount; ++a) {
            AnimClip& clip = mesh->m_anims[a];
            uint32_t nameLen = 0; readU32(nameLen);
            clip.name.resize(nameLen);
            if (nameLen) file.read(clip.name.data(), nameLen);
            readF(clip.duration);
            readF(clip.tps);
            uint32_t channelCount = 0; readU32(channelCount);
            clip.channels.resize(channelCount);
            for (uint32_t c = 0; c < channelCount; ++c) {
                AnimChannel& ch = clip.channels[c];
                int32_t bidx = -1; file.read(reinterpret_cast<char*>(&bidx), 4);
                ch.bone = bidx;
                uint32_t n = 0;
                readU32(n); ch.pos.resize(n);
                for (uint32_t k = 0; k < n; ++k) { float t, x, y, z; readF(t); readF(x); readF(y); readF(z); ch.pos[k] = { t, glm::vec3(x, y, z) }; }
                readU32(n); ch.rot.resize(n);
                for (uint32_t k = 0; k < n; ++k) { float t, x, y, z, w; readF(t); readF(x); readF(y); readF(z); readF(w); ch.rot[k] = { t, glm::quat(w, x, y, z) }; }
                readU32(n); ch.scl.resize(n);
                for (uint32_t k = 0; k < n; ++k) { float t, x, y, z; readF(t); readF(x); readF(y); readF(z); ch.scl[k] = { t, glm::vec3(x, y, z) }; }
            }
        }

        // Precompute the rest-pose skinning matrices (bones are pre-order, so a
        // parent is always processed before its children).
        std::vector<glm::mat4> global(mesh->m_bones.size());
        mesh->m_bindPose.resize(mesh->m_bones.size());
        for (size_t i = 0; i < mesh->m_bones.size(); ++i) {
            const ModelBone& b = mesh->m_bones[i];
            global[i] = (b.parent < 0) ? b.localBind : global[b.parent] * b.localBind;
            mesh->m_bindPose[i] = mesh->m_globalInverse * global[i] * b.offset;
        }
    }

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
