#pragma once
#include <cstdint>

static constexpr uint32_t kEmdlMagic   = 0x454D444C;
static constexpr uint32_t kEmdlVersion = 3;            // v3: optional skeleton + animations

static constexpr int kEmdlMaxBoneInfluences = 4;
static constexpr int kEmdlMaxBoneNameLen    = 64;

#pragma pack(push, 1)
struct EmdlHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t vertexCount;
    uint32_t indexCount;
    float    aabbMin[3];
    float    aabbMax[3];
    uint32_t boneCount;        // 0 => static mesh (no skin/skeleton/animation data)
    uint32_t animCount;
    float    globalInverse[16];// inverse of the scene root transform (skinning)
};

struct EmdlVertex {
    float pos[3];
    float normal[3];
    float uv[2];
    float tangent[3];
    float bitangent[3];
};

// Per-vertex skin influences. Present (parallel to the vertex array) only when
// boneCount > 0. Unused slots have weight 0.
struct EmdlSkin {
    int32_t boneIds[kEmdlMaxBoneInfluences];
    float   weights[kEmdlMaxBoneInfluences];
};

// A skeleton bone: name (matches animation channels), parent index (-1 = root),
// the inverse-bind ("offset") matrix and the node's default local transform
// (used for bones an animation clip doesn't move). All matrices column-major.
struct EmdlBone {
    char    name[kEmdlMaxBoneNameLen];
    int32_t parent;
    float   offset[16];
    float   localBind[16];
};
#pragma pack(pop)

// ── Variable-length tail (only when boneCount > 0), written/read field by field
// after the fixed arrays (vertices, indices, EmdlSkin[vertexCount],
// EmdlBone[boneCount]):
//
//   for each of animCount animations:
//     u32 nameLen, char name[nameLen]
//     f32 durationTicks
//     f32 ticksPerSecond
//     u32 channelCount
//     for each channel:
//       i32 boneIndex
//       u32 posKeyCount; { f32 t; f32 v[3]; } * posKeyCount
//       u32 rotKeyCount; { f32 t; f32 q[4]; } * rotKeyCount   // x,y,z,w
//       u32 sclKeyCount; { f32 t; f32 v[3]; } * sclKeyCount
