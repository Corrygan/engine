#pragma once
#include <string>

enum class PrimitiveType {
    Cube,
    Sphere,
    Camera,
    Light,
    Empty,
    Plane
};

inline const char* ToString(PrimitiveType type) {
    switch (type) {
        case PrimitiveType::Cube:   return "Cube";
        case PrimitiveType::Sphere: return "Sphere";
        case PrimitiveType::Camera: return "Camera";
        case PrimitiveType::Light:  return "Light";
        case PrimitiveType::Empty:  return "Empty";
        case PrimitiveType::Plane:  return "Plane";
    }
    return "Unknown";
}

struct GameObject {
    std::string name;
    PrimitiveType type = PrimitiveType::Cube;
    float position[3] = { 0.0f, 0.0f, 0.0f };
    float rotation[3] = { 0.0f, 0.0f, 0.0f };
    float scale[3] = { 1.0f, 1.0f, 1.0f };
};