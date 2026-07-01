#include "ModelImporter.h"
#include "EmdlFormat.h"
#include "MetaFile.h"
#include "../Core/Guid.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <filesystem>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cfloat>
#include <unordered_map>
#include <functional>
#include <string>

namespace {
    // assimp matrices are row-major; convert to column-major (GL/GLM) float[16].
    void AiMatToColumnMajor(const aiMatrix4x4& m, float* o) {
        o[0]  = m.a1; o[1]  = m.b1; o[2]  = m.c1; o[3]  = m.d1;
        o[4]  = m.a2; o[5]  = m.b2; o[6]  = m.c2; o[7]  = m.d2;
        o[8]  = m.a3; o[9]  = m.b3; o[10] = m.c3; o[11] = m.d3;
        o[12] = m.a4; o[13] = m.b4; o[14] = m.c4; o[15] = m.d4;
    }
}

namespace fs = std::filesystem;

std::string ModelImporter::EmdlPathFor(const std::string& sourcePath) {
    fs::path p(sourcePath);
    return (p.parent_path() / p.stem()).string() + ".emdl";
}

bool ModelImporter::NeedsReimport(const std::string& sourcePath) {
    const std::string emdlPath = EmdlPathFor(sourcePath);
    if (!fs::exists(emdlPath)) return true;

    {
        std::ifstream f(emdlPath, std::ios::binary);
        EmdlHeader header{};
        if (!f.read(reinterpret_cast<char*>(&header), sizeof(header)) ||
            header.magic != kEmdlMagic || header.version != kEmdlVersion)
            return true;
    }

    MetaFile meta;
    if (!MetaFile::Load(sourcePath + ".meta", meta)) return true;

    auto it = meta.extra.find("source_mtime");
    if (it == meta.extra.end()) return true;

    uint64_t storedMtime  = std::stoull(it->second);
    uint64_t currentMtime = static_cast<uint64_t>(
        fs::last_write_time(sourcePath).time_since_epoch().count());
    return currentMtime != storedMtime;
}

std::string ModelImporter::Import(const std::string& sourcePath) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(sourcePath,
        aiProcess_Triangulate         |
        aiProcess_GenSmoothNormals    |
        aiProcess_CalcTangentSpace    |
        aiProcess_FlipUVs             |
        aiProcess_LimitBoneWeights    |
        aiProcess_JoinIdenticalVertices);

    if (!scene || !scene->HasMeshes()) return "";

    std::vector<EmdlVertex> vertices;
    std::vector<uint32_t>   indices;
    std::vector<uint32_t>   meshBase;   // first global vertex index of each mesh
    float aabbMin[3] = {  FLT_MAX,  FLT_MAX,  FLT_MAX };
    float aabbMax[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

    for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
        const aiMesh* mesh = scene->mMeshes[m];
        const uint32_t base = static_cast<uint32_t>(vertices.size());
        meshBase.push_back(base);

        for (unsigned int v = 0; v < mesh->mNumVertices; ++v) {
            EmdlVertex vert{};
            vert.pos[0] = mesh->mVertices[v].x;
            vert.pos[1] = mesh->mVertices[v].y;
            vert.pos[2] = mesh->mVertices[v].z;

            if (mesh->HasNormals()) {
                vert.normal[0] = mesh->mNormals[v].x;
                vert.normal[1] = mesh->mNormals[v].y;
                vert.normal[2] = mesh->mNormals[v].z;
            }
            if (mesh->HasTextureCoords(0)) {
                vert.uv[0] = mesh->mTextureCoords[0][v].x;
                vert.uv[1] = mesh->mTextureCoords[0][v].y;
            }
            if (mesh->HasTangentsAndBitangents()) {
                vert.tangent[0]   = mesh->mTangents[v].x;
                vert.tangent[1]   = mesh->mTangents[v].y;
                vert.tangent[2]   = mesh->mTangents[v].z;
                vert.bitangent[0] = mesh->mBitangents[v].x;
                vert.bitangent[1] = mesh->mBitangents[v].y;
                vert.bitangent[2] = mesh->mBitangents[v].z;
            }

            for (int i = 0; i < 3; ++i) {
                aabbMin[i] = std::min(aabbMin[i], vert.pos[i]);
                aabbMax[i] = std::max(aabbMax[i], vert.pos[i]);
            }
            vertices.push_back(vert);
        }

        for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
            const aiFace& face = mesh->mFaces[f];
            for (unsigned int i = 0; i < face.mNumIndices; ++i)
                indices.push_back(base + face.mIndices[i]);
        }
    }

    // ── Skeleton + skin (only when the model is rigged or animated) ────────────
    bool hasBones = scene->mNumAnimations > 0;
    for (unsigned int m = 0; m < scene->mNumMeshes && !hasBones; ++m)
        if (scene->mMeshes[m]->mNumBones > 0) hasBones = true;

    std::vector<EmdlBone> bones;
    std::vector<EmdlSkin> skins;
    std::unordered_map<std::string, int> nodeIndex;
    float globalInverse[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    {
        aiMatrix4x4 gi = scene->mRootNode->mTransformation; gi.Inverse();
        AiMatToColumnMajor(gi, globalInverse);
    }

    if (hasBones) {
        // Flatten the whole node hierarchy in pre-order (parent before child) so
        // intermediate (non-skinning) nodes still drive the bone chain.
        std::function<void(const aiNode*, int)> visit = [&](const aiNode* node, int parent) {
            int idx = (int)bones.size();
            EmdlBone b{};
            std::string name = node->mName.C_Str();
            std::strncpy(b.name, name.c_str(), kEmdlMaxBoneNameLen - 1);
            b.parent = parent;
            AiMatToColumnMajor(node->mTransformation, b.localBind);
            b.offset[0] = b.offset[5] = b.offset[10] = b.offset[15] = 1.0f;  // identity
            bones.push_back(b);
            nodeIndex[name] = idx;
            for (unsigned int c = 0; c < node->mNumChildren; ++c)
                visit(node->mChildren[c], idx);
        };
        visit(scene->mRootNode, -1);

        skins.assign(vertices.size(), EmdlSkin{});
        auto addWeight = [&](uint32_t vtx, int boneIdx, float w) {
            if (w <= 0.0f || vtx >= skins.size()) return;
            EmdlSkin& s = skins[vtx];
            int slot = -1;
            for (int i = 0; i < kEmdlMaxBoneInfluences; ++i)
                if (s.weights[i] == 0.0f) { slot = i; break; }
            if (slot < 0) {
                int mn = 0;
                for (int i = 1; i < kEmdlMaxBoneInfluences; ++i)
                    if (s.weights[i] < s.weights[mn]) mn = i;
                if (w <= s.weights[mn]) return;
                slot = mn;
            }
            s.boneIds[slot] = boneIdx;
            s.weights[slot] = w;
        };

        for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
            const aiMesh* mesh = scene->mMeshes[m];
            for (unsigned int bi = 0; bi < mesh->mNumBones; ++bi) {
                const aiBone* bone = mesh->mBones[bi];
                auto it = nodeIndex.find(bone->mName.C_Str());
                if (it == nodeIndex.end()) continue;
                int idx = it->second;
                AiMatToColumnMajor(bone->mOffsetMatrix, bones[idx].offset);
                for (unsigned int w = 0; w < bone->mNumWeights; ++w)
                    addWeight(meshBase[m] + bone->mWeights[w].mVertexId, idx, bone->mWeights[w].mWeight);
            }
        }

        for (EmdlSkin& s : skins) {
            float sum = s.weights[0] + s.weights[1] + s.weights[2] + s.weights[3];
            if (sum > 0.0f) for (int i = 0; i < kEmdlMaxBoneInfluences; ++i) s.weights[i] /= sum;
        }
    }

    const std::string emdlPath = EmdlPathFor(sourcePath);
    {
        std::ofstream file(emdlPath, std::ios::binary);
        if (!file.is_open()) return "";

        EmdlHeader header{};
        header.magic       = kEmdlMagic;
        header.version     = kEmdlVersion;
        header.vertexCount = static_cast<uint32_t>(vertices.size());
        header.indexCount  = static_cast<uint32_t>(indices.size());
        std::memcpy(header.aabbMin, aabbMin, sizeof(aabbMin));
        std::memcpy(header.aabbMax, aabbMax, sizeof(aabbMax));
        header.boneCount   = hasBones ? static_cast<uint32_t>(bones.size()) : 0u;
        header.animCount   = hasBones ? scene->mNumAnimations : 0u;
        std::memcpy(header.globalInverse, globalInverse, sizeof(globalInverse));

        file.write(reinterpret_cast<const char*>(&header),         sizeof(header));
        file.write(reinterpret_cast<const char*>(vertices.data()), vertices.size() * sizeof(EmdlVertex));
        file.write(reinterpret_cast<const char*>(indices.data()),  indices.size()  * sizeof(uint32_t));

        if (hasBones) {
            file.write(reinterpret_cast<const char*>(skins.data()), skins.size() * sizeof(EmdlSkin));
            file.write(reinterpret_cast<const char*>(bones.data()), bones.size() * sizeof(EmdlBone));

            auto wU32 = [&](uint32_t v) { file.write(reinterpret_cast<const char*>(&v), 4); };
            auto wF   = [&](float v)    { file.write(reinterpret_cast<const char*>(&v), 4); };
            for (unsigned int a = 0; a < scene->mNumAnimations; ++a) {
                const aiAnimation* anim = scene->mAnimations[a];
                std::string name = anim->mName.C_Str();
                wU32(static_cast<uint32_t>(name.size()));
                file.write(name.data(), static_cast<std::streamsize>(name.size()));
                wF(static_cast<float>(anim->mDuration));
                wF(anim->mTicksPerSecond != 0.0 ? static_cast<float>(anim->mTicksPerSecond) : 25.0f);
                wU32(anim->mNumChannels);
                for (unsigned int c = 0; c < anim->mNumChannels; ++c) {
                    const aiNodeAnim* ch = anim->mChannels[c];
                    auto it = nodeIndex.find(ch->mNodeName.C_Str());
                    int32_t bidx = (it != nodeIndex.end()) ? it->second : -1;
                    file.write(reinterpret_cast<const char*>(&bidx), 4);
                    wU32(ch->mNumPositionKeys);
                    for (unsigned int k = 0; k < ch->mNumPositionKeys; ++k) {
                        wF((float)ch->mPositionKeys[k].mTime);
                        wF(ch->mPositionKeys[k].mValue.x); wF(ch->mPositionKeys[k].mValue.y); wF(ch->mPositionKeys[k].mValue.z);
                    }
                    wU32(ch->mNumRotationKeys);
                    for (unsigned int k = 0; k < ch->mNumRotationKeys; ++k) {
                        const aiQuaternion& q = ch->mRotationKeys[k].mValue;
                        wF((float)ch->mRotationKeys[k].mTime);
                        wF(q.x); wF(q.y); wF(q.z); wF(q.w);
                    }
                    wU32(ch->mNumScalingKeys);
                    for (unsigned int k = 0; k < ch->mNumScalingKeys; ++k) {
                        wF((float)ch->mScalingKeys[k].mTime);
                        wF(ch->mScalingKeys[k].mValue.x); wF(ch->mScalingKeys[k].mValue.y); wF(ch->mScalingKeys[k].mValue.z);
                    }
                }
            }
        }
    }

    const std::string metaPath = sourcePath + ".meta";
    MetaFile meta;
    MetaFile::Load(metaPath, meta);
    if (!meta.guid.IsValid()) meta.guid = Guid::Generate();
    meta.type = "Model";
    meta.extra["source_mtime"] = std::to_string(static_cast<uint64_t>(
        fs::last_write_time(sourcePath).time_since_epoch().count()));
    meta.Save(metaPath);

    return emdlPath;
}
