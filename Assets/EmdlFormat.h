#pragma once
#include <cstdint>

static constexpr uint32_t kEmdlMagic   = 0x454D444C;
static constexpr uint32_t kEmdlVersion = 2; // v2: added tangent + bitangent

#pragma pack(push, 1)
struct EmdlHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t vertexCount;
    uint32_t indexCount;
    float    aabbMin[3];
    float    aabbMax[3];
};

struct EmdlVertex {
    float pos[3];
    float normal[3];
    float uv[2];
    float tangent[3];
    float bitangent[3];
};
#pragma pack(pop)
