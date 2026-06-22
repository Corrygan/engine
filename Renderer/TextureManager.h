#pragma once
#include "Texture.h"
#include <string>
#include <unordered_map>

class TextureManager {
public:
    static Texture* GetOrLoad(const std::string& path);
    static void     Invalidate(const std::string& path);
    static void     Clear();

private:
    static std::unordered_map<std::string, Texture*> s_cache;
};
