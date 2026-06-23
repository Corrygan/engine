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
#include "../Scene/GameObject.h"
#include "../Renderer/EditorCamera.h"

struct AssetItem {
    std::string path;
    std::string name;
    std::string ext;
    bool        isDir = false;
};

class SceneRenderer;

struct ConsoleMessage {
    enum Type { Info, Warning, Error, Success };
    Type type;
    std::string text;
    std::string timestamp;
};

class EditorUI {
public:
    EditorUI();
    ~EditorUI();

    bool Initialize(GLFWwindow* window);
    void Shutdown();
    void Render();

    void AddGameObject(const std::string& name, PrimitiveType type = PrimitiveType::Empty);
    void AddModelObject(const std::string& emdlPath);
    void SelectGameObject(int index);
    GameObject* GetSelectedObject();

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
    void RenderAssetBrowser();
    void RenderGameObjectNode(GameObject& obj, int index);
    void RenderTransformEditor(GameObject* obj);
    void RenderUnsavedChangesDialog();
    void AddConsoleMessage(const std::string& message, ConsoleMessage::Type type);
    bool SaveCurrentScene();

private:
    GLFWwindow* m_window = nullptr;
    SceneRenderer* m_sceneRenderer = nullptr;
    EditorCamera m_camera;
    float m_viewportDragAccum = 0.0f;
    std::vector<GameObject> m_gameObjects;
    int m_selectedObjectIndex = -1;
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

    bool m_sceneDirty = false;
    bool m_showUnsavedChangesDialog = false;
    std::string m_currentScenePath;

    MaterialPreviewRenderer* m_previewRenderer = nullptr;
    MaterialNodeEditor*      m_nodeEditor      = nullptr;

public:
    bool m_pendingMinimize = false;
    bool m_pendingMaximize = false;
};
