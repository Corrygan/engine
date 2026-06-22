#include "MaterialManager.h"

std::unordered_map<std::string, Material> MaterialManager::s_cache;

Material* MaterialManager::GetOrLoad(const std::string& path) {
    if (path.empty()) return nullptr;
    auto it = s_cache.find(path);
    if (it != s_cache.end()) return &it->second;
    Material mat;
    if (!Material::Load(path, mat)) return nullptr;
    s_cache[path] = mat;
    return &s_cache[path];
}

void MaterialManager::Invalidate(const std::string& path) {
    s_cache.erase(path);
}

void MaterialManager::Clear() {
    s_cache.clear();
}
