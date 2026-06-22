#include "TextureManager.h"

std::unordered_map<std::string, Texture*> TextureManager::s_cache;

Texture* TextureManager::GetOrLoad(const std::string& path) {
    if (path.empty()) return nullptr;
    auto it = s_cache.find(path);
    if (it != s_cache.end()) return it->second;
    Texture* tex = Texture::LoadFromFile(path);
    s_cache[path] = tex;
    return tex;
}

void TextureManager::Invalidate(const std::string& path) {
    auto it = s_cache.find(path);
    if (it != s_cache.end()) {
        delete it->second;
        s_cache.erase(it);
    }
}

void TextureManager::Clear() {
    for (auto& [path, tex] : s_cache) delete tex;
    s_cache.clear();
}
