#pragma once
#include <string>

struct Material {
    float color[4]          = { 0.55f, 0.58f, 0.65f, 1.0f };
    float metallic          = 0.0f;
    float roughness         = 0.5f;
    float emissiveColor[3]  = { 0.0f, 0.0f, 0.0f };
    float emissiveIntensity = 0.0f;

    std::string albedoTexture;
    std::string normalTexture;
    std::string ormTexture;

    bool Save(const std::string& path) const;
    static bool Load(const std::string& path, Material& out);
};
