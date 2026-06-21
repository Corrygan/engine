#pragma once
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "glfw3.h"
#include <string>
#include <vector>
#include "../Scene/GameObject.h"
#include "../Renderer/EditorCamera.h"

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

    void AddGameObject(const std::string& name);
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

    void RenderGameObjectNode(GameObject& obj, int index);
    void RenderTransformEditor(GameObject* obj);
    void AddConsoleMessage(const std::string& message, ConsoleMessage::Type type);

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

    bool m_showHierarchy = true;
    bool m_showInspector = true;
    bool m_showConsole = true;

    float m_viewportWidth = 800.0f;
    float m_viewportHeight = 600.0f;
    int m_objectCounter = 0;
};