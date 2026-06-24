#pragma once
#include "../Renderer/MaterialGraph.h"
#include "../Renderer/NodeCompiler.h"
#include <string>
#include <functional>

class Shader;
class MaterialPreviewRenderer;
class SceneRenderer;  // forward-declared, full header in .cpp

class MaterialNodeEditor {
public:
    MaterialNodeEditor();
    ~MaterialNodeEditor();

    void Open(const std::string& matPath, MaterialPreviewRenderer* preview,
              SceneRenderer* sceneRenderer = nullptr);
    void Render();
    bool IsOpen() const { return m_open; }

    // Compile + register graph for matPath without opening the editor UI.
    // Used when a material is assigned to an object or scanned in the browser.
    // If `preview` is given, also refreshes that material's cached thumbnail.
    void EnsureCompiled(const std::string& matPath, SceneRenderer* sr,
                        MaterialPreviewRenderer* preview = nullptr);

    Shader* GetCompiledShader() const { return m_compiledShader; }
    const std::string& GetMatPath() const { return m_matPath; }

    // Called after the material file is renamed (oldPath, newPath) so the owner
    // can re-point GameObjects and refresh the asset browser.
    void SetRenameHandler(std::function<void(const std::string&, const std::string&)> cb) {
        m_onRename = std::move(cb);
    }

private:
    void RenderToolbar();
    void RenderCanvas();
    void RenderSidebar();
    void RenderNodeProperties(GraphNode& node);
    void RenderAddNodeMenu();

    void TryCompile();
    void SaveGraph();
    void LoadGraph();
    void CommitNow();   // compile + autosave immediately (for discrete edits)
    void Close();       // autosave then close
    void RenameTo(const std::string& newStem);   // rename material file + sync state

    bool              m_open          = false;
    std::string       m_matPath;
    std::string       m_graphPath;
    MaterialGraph     m_graph;

    int               m_selectedNode  = -1;
    int               m_ctxNode       = -1;   // right-click target (node)
    int               m_ctxLink       = -1;   // right-click target (link)

    // Compilation
    Shader*           m_compiledShader = nullptr;
    CompiledShader    m_compiled;
    bool              m_dirty          = false;
    float             m_compileTimer   = 0.0f;
    static constexpr float kCompileDelay = 0.2f;   // debounce for continuous drags

    // Add-node popup
    bool              m_showAddMenu    = false;
    float             m_addMenuX       = 0.0f;
    float             m_addMenuY       = 0.0f;

    MaterialPreviewRenderer* m_preview       = nullptr;
    SceneRenderer*           m_sceneRenderer = nullptr;
    uint32_t                 m_previewTex    = 0;

    // Editable material name (filename stem), synced on Open / rename
    char              m_nameBuf[128] = {0};

    std::function<void(const std::string&, const std::string&)> m_onRename;

    // imnodes context pointer (opaque)
    void*             m_imnodesCtx = nullptr;
};
