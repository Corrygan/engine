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
        aiProcess_JoinIdenticalVertices);

    if (!scene || !scene->HasMeshes()) return "";

    std::vector<EmdlVertex> vertices;
    std::vector<uint32_t>   indices;
    float aabbMin[3] = {  FLT_MAX,  FLT_MAX,  FLT_MAX };
    float aabbMax[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

    for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
        const aiMesh* mesh = scene->mMeshes[m];
        const uint32_t base = static_cast<uint32_t>(vertices.size());

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

        file.write(reinterpret_cast<const char*>(&header),         sizeof(header));
        file.write(reinterpret_cast<const char*>(vertices.data()), vertices.size() * sizeof(EmdlVertex));
        file.write(reinterpret_cast<const char*>(indices.data()),  indices.size()  * sizeof(uint32_t));
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
