#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>
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

    // Render the preview sphere with a node-compiled shader. Returns the live
    // framebuffer texture (valid until the next render). Used by the node
    // editor's sidebar for real-time feedback.
    uint32_t RenderWithShader(Shader* shader,
                 const std::vector<std::pair<std::string, std::string>>& texBindings);

    // Same render, but copies the result into the cached thumbnail for `path`
    // so the asset browser / inspector swatch reflect node-graph edits.
    void     UpdatePreviewWithShader(const std::string& path, Shader* shader,
                 const std::vector<std::pair<std::string, std::string>>& texBindings);

private:
    uint32_t CopyToTexture(uint32_t srcTex);

    Framebuffer* m_fb     = nullptr;
    Shader*      m_shader = nullptr;
    SphereMesh*  m_sphere = nullptr;

    std::unordered_map<std::string, uint32_t> m_cache;
};
