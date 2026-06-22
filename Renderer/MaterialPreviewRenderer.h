#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include "Material.h"

class Framebuffer;
class Shader;
class SphereMesh;

class MaterialPreviewRenderer {
public:
    MaterialPreviewRenderer();
    ~MaterialPreviewRenderer();

    uint32_t Render(Material* mat);

    // Cached thumbnail: renders once, stores in a standalone texture
    uint32_t GetPreview(Material* mat, const std::string& path);
    void     InvalidatePreview(const std::string& path);
    void     ClearCache();

private:
    uint32_t CopyToTexture(uint32_t srcTex);

    Framebuffer* m_fb     = nullptr;
    Shader*      m_shader = nullptr;
    SphereMesh*  m_sphere = nullptr;

    std::unordered_map<std::string, uint32_t> m_cache;
};
