#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <utility>
#include <glm/glm.hpp>
#include "../Scene/GameObject.h"

class Framebuffer;
class Shader;
class PrimitiveMesh;
class ModelMesh;
class Grid;

struct NodeShaderEntry {
    Shader*     shader = nullptr;
    // Each pair: (glsl uniform name, texture file path)
    std::vector<std::pair<std::string, std::string>> texBindings;
};

class SceneRenderer {
public:
    SceneRenderer();
    ~SceneRenderer();

    uint32_t Render(const std::vector<GameObject>& objects, int selectedIndex,
        int width, int height,
        const glm::mat4& view, const glm::mat4& projection);

    void RegisterNodeShader(const std::string& matPath,
                            Shader* shader,
                            std::vector<std::pair<std::string,std::string>> texBindings);
    void UnregisterNodeShader(const std::string& matPath);

private:
    PrimitiveMesh* GetMeshForType(PrimitiveType type) const;
    ModelMesh*     GetOrLoadModel(const std::string& emdlPath);

    Framebuffer* m_framebuffer = nullptr;
    Shader* m_shader = nullptr;
    Shader* m_lineShader = nullptr;
    Shader* m_outlineShader = nullptr;
    PrimitiveMesh* m_cube = nullptr;
    PrimitiveMesh* m_sphere = nullptr;
    PrimitiveMesh* m_plane = nullptr;
    Grid* m_grid = nullptr;

    std::unordered_map<std::string, ModelMesh*>     m_modelCache;
    std::unordered_map<std::string, NodeShaderEntry> m_nodeShaders;
};