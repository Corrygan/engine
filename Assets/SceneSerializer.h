#pragma once
#include <string>
#include "../Scene/Scene.h"

class SceneSerializer {
public:
    static bool Save(const std::string& path, const Scene& scene);
    static bool Load(const std::string& path, Scene& scene);
};
