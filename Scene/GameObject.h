#pragma once
#include <string>

enum class PrimitiveType {
    Cube,
    Sphere,
    Camera,
    Light,
    Empty,
    Plane,
    Model
};

inline const char* ToString(PrimitiveType type) {
    switch (type) {
        case PrimitiveType::Cube:   return "Cube";
        case PrimitiveType::Sphere: return "Sphere";
        case PrimitiveType::Camera: return "Camera";
        case PrimitiveType::Light:  return "Light";
        case PrimitiveType::Empty:  return "Empty";
        case PrimitiveType::Plane:  return "Plane";
        case PrimitiveType::Model:  return "Model";
    }
    return "Unknown";
}

enum class LightType {
    Directional,
    Point,
    Spot
};

inline const char* ToString(LightType t) {
    switch (t) {
        case LightType::Directional: return "Directional";
        case LightType::Point:       return "Point";
        case LightType::Spot:        return "Spot";
    }
    return "Unknown";
}

struct GameObject {
    std::string name;
    PrimitiveType type = PrimitiveType::Cube;
    std::string modelPath;
    std::string materialPath;
    float aabbMin[3]  = { -0.5f, -0.5f, -0.5f };
    float aabbMax[3]  = {  0.5f,  0.5f,  0.5f };
    float position[3] = { 0.0f, 0.0f, 0.0f };
    float rotQuat[4]  = { 0.0f, 0.0f, 0.0f, 1.0f }; // x y z w  (identity)
    float scale[3]    = { 1.0f, 1.0f, 1.0f };

    // Light properties (used when type == PrimitiveType::Light)
    int   lightType      = 0;                    // 0=Directional, 1=Point, 2=Spot
    float lightColor[3]  = { 1.0f, 1.0f, 1.0f };
    float lightIntensity = 3.0f;
    float lightRange     = 10.0f;                // point/spot falloff distance
    float spotInnerDeg   = 25.0f;                // spot cone inner angle (degrees)
    float spotOuterDeg   = 35.0f;                // spot cone outer angle (degrees)
};
