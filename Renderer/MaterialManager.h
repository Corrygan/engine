#pragma once
#include "Material.h"
#include <string>
#include <unordered_map>

class MaterialManager {
public:
    static Material* GetOrLoad(const std::string& path);
    static void      Invalidate(const std::string& path);
    static void      Clear();

private:
    static std::unordered_map<std::string, Material> s_cache;
};
