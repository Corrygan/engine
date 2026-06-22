#pragma once
#include <string>
#include <vector>
#include "../Scene/GameObject.h"

class SceneSerializer {
public:
    static bool Save(const std::string& path, const std::vector<GameObject>& objects);
    static bool Load(const std::string& path, std::vector<GameObject>& outObjects);
};
