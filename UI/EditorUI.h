#pragma once
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "ImGuizmo.h"
#include "glfw3.h"
#include "../Renderer/MaterialPreviewRenderer.h"
#include "MaterialNodeEditor.h"
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include "../Scene/Scene.h"
#include "../Renderer/EditorCamera.h"

struct AssetItem {
    std::string path;
    std::string name;
    std::string ext;
    bool        isDir = false;
};

class SceneRenderer;
class LuaScripting;

struct ConsoleMessage {
    enum Type { Info, Warning, Error, Success };
    Type type;
    std::string text;
    std::string timestamp;
};

enum class PlayState { Editing, Playing, Paused };

class EditorUI {
public:
    EditorUI();
    ~EditorUI();

    bool Initialize(GLFWwindow* window);
    void Shutdown();
    void Render();

    void AddGameObject(const std::string& name, PrimitiveType type = PrimitiveType::Empty);
    void AddModelObject(const std::string& emdlPath);
    void SelectEntity(entt::entity e);

    void LogInfo(const std::string& message);
    void LogWarning(const std::string& message);
    void LogError(const std::string& message);
    void LogSuccess(const std::string& message);

    void SetViewportSize(float width, float height);

private:
    void RenderDockspace();
    void RenderMenuBar();
    void RenderHierarchy();
    void RenderViewport();
    void RenderInspector();
    void RenderConsole();

    void RenderTitleBar();
    void RenderViewportToolbar();    // icon-only bar above the viewport (play/render/settings)
    void RenderViewportTimeline();   // placeholder timeline strip below the viewport

    // Play mode
    void EnterPlayMode();
    void ExitPlayMode();
    void TogglePause();
    void UpdatePlayMode();           // per-frame game update while playing
    void RenderAssetBrowser();
    void RenderEntityNode(entt::entity e);
    entt::entity DuplicateEntity(entt::entity src);
    void ImportModelAsync(const std::string& sourcePath);   // assimp on a worker, place on main

    static constexpr float kViewportToolbarH  = 38.0f;
    static constexpr float kViewportTimelineH = 96.0f;
    void RenderTransformEditor(entt::entity e);
    void RenderUnsavedChangesDialog();
    void RequestClose();   // X/Exit: prompt if dirty, else close
    void AddConsoleMessage(const std::string& message, ConsoleMessage::Type type);
    bool SaveCurrentScene();

private:
    GLFWwindow* m_window = nullptr;
    SceneRenderer* m_sceneRenderer = nullptr;
    EditorCamera m_camera;
    float m_viewportDragAccum = 0.0f;
    entt::entity m_selected = entt::null;
    std::vector<ConsoleMessage> m_consoleMessages;
    bool m_autoScrollConsole = true;
    bool m_shouldAutoScroll = false;

    bool m_showHierarchy     = true;
    bool m_showInspector     = true;
    bool m_showConsole       = true;
    bool m_showAssetBrowser  = true;

    float m_viewportWidth = 800.0f;
    float m_viewportHeight = 600.0f;
    int m_objectCounter = 0;

    ImGuizmo::OPERATION m_gizmoOp   = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE      m_gizmoMode = ImGuizmo::WORLD;

    std::string             m_assetBrowserPath     = "Assets";
    std::string             m_assetBrowserSelected;
    std::vector<AssetItem>  m_assetBrowserItems;
    bool                    m_assetBrowserDirty    = true;
    std::unordered_set<std::string> m_compiledGraphs;   // node materials already precompiled

    bool m_sceneDirty = false;
    bool m_showUnsavedChangesDialog = false;
    std::string m_currentScenePath;

    // The ECS registry is the editor's single source of truth.
    Scene                       m_scene;

    // Play mode
    PlayState               m_playState = PlayState::Editing;
    Scene                   m_playBackup;          // full snapshot, restored on Stop
    std::unordered_map<entt::entity, entt::entity> m_playMap;   // live entity → backup entity
    float                   m_playTime = 0.0f;

    MaterialPreviewRenderer* m_previewRenderer = nullptr;
    MaterialNodeEditor*      m_nodeEditor      = nullptr;
    LuaScripting*            m_lua             = nullptr;

public:
    bool m_pendingMinimize = false;
    bool m_pendingMaximize = false;
};
