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
};
