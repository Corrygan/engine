#define _CRT_SECURE_NO_WARNINGS
#include "EditorUI.h"
#include "imgui_internal.h"
#include "../Assets/SceneSerializer.h"
#include "../Assets/AssetDatabase.h"
#include "../Renderer/SceneRenderer.h"
#include "../Renderer/Picking.h"
#include <iostream>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <cstdint>
#include <cmath>

EditorUI::EditorUI() {
}

EditorUI::~EditorUI() {
    Shutdown();
}

bool EditorUI::Initialize(GLFWwindow* window) {
    m_window = window;
    if (!m_window) {
        std::cerr << "EditorUI: Invalid window handle!" << std::endl;
        return false;
    }

    // К этому моменту gladLoadGL() в main.cpp уже отработал, так что можно
    // безопасно создавать GL-ресурсы.
    m_sceneRenderer = new SceneRenderer();

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();

    // --- Геометрия / отступы ---
    style.WindowRounding = 0.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 4.0f;

    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;

    style.WindowPadding = ImVec2(10.0f, 10.0f);
    style.FramePadding = ImVec2(8.0f, 5.0f);
    style.ItemSpacing = ImVec2(8.0f, 7.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 6.0f);
    style.IndentSpacing = 18.0f;
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 10.0f;

    // --- Цветовая палитра: тёмная база + один синий акцент ---
    ImVec4* colors = style.Colors;
    const ImVec4 accent = ImVec4(0.30f, 0.55f, 0.95f, 1.0f);

    colors[ImGuiCol_Text] = ImVec4(0.92f, 0.93f, 0.94f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.53f, 1.00f);

    colors[ImGuiCol_WindowBg] = ImVec4(0.110f, 0.114f, 0.122f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.110f, 0.114f, 0.122f, 0.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.085f, 0.088f, 0.095f, 0.98f);
    colors[ImGuiCol_Border] = ImVec4(1.00f, 1.00f, 1.00f, 0.07f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.165f, 0.177f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.205f, 0.220f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.245f, 0.260f, 1.00f);

    colors[ImGuiCol_TitleBg] = ImVec4(0.07f, 0.07f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.07f, 0.07f, 0.08f, 0.60f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.09f, 0.09f, 0.10f, 1.00f);

    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.07f, 0.07f, 0.08f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.25f, 0.25f, 0.27f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.32f, 0.32f, 0.34f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.40f, 0.40f, 0.42f, 1.00f);

    colors[ImGuiCol_CheckMark] = accent;
    colors[ImGuiCol_SliderGrab] = ImVec4(accent.x, accent.y, accent.z, 0.75f);
    colors[ImGuiCol_SliderGrabActive] = accent;

    colors[ImGuiCol_Button] = ImVec4(0.18f, 0.185f, 0.200f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.23f, 0.235f, 0.250f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.27f, 0.275f, 0.290f, 1.00f);

    // Header* красит Selectable / TreeNode / CollapsingHeader — используем
    // акцент здесь, чтобы выделение объекта в сцене сразу было заметно.
    colors[ImGuiCol_Header] = ImVec4(accent.x, accent.y, accent.z, 0.45f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(accent.x, accent.y, accent.z, 0.65f);
    colors[ImGuiCol_HeaderActive] = ImVec4(accent.x, accent.y, accent.z, 0.85f);

    colors[ImGuiCol_Separator] = ImVec4(1.00f, 1.00f, 1.00f, 0.08f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(accent.x, accent.y, accent.z, 0.60f);
    colors[ImGuiCol_SeparatorActive] = accent;

    colors[ImGuiCol_ResizeGrip] = ImVec4(accent.x, accent.y, accent.z, 0.25f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(accent.x, accent.y, accent.z, 0.55f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(accent.x, accent.y, accent.z, 0.85f);

    colors[ImGuiCol_Tab] = ImVec4(0.13f, 0.135f, 0.145f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(accent.x, accent.y, accent.z, 0.55f);
    colors[ImGuiCol_TabSelected] = ImVec4(accent.x, accent.y, accent.z, 0.80f);
    colors[ImGuiCol_TabDimmed] = ImVec4(0.10f, 0.10f, 0.11f, 1.00f);
    colors[ImGuiCol_TabDimmedSelected] = ImVec4(accent.x, accent.y, accent.z, 0.40f);

    colors[ImGuiCol_DockingPreview] = ImVec4(accent.x, accent.y, accent.z, 0.50f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.09f, 0.09f, 0.10f, 1.00f);

    return true;
}

void EditorUI::Shutdown() {
    delete m_sceneRenderer;
    m_sceneRenderer = nullptr;
}

void EditorUI::Render() {
    if (!m_window) return;
    RenderDockspace();
    RenderMenuBar();
    RenderHierarchy();
    RenderViewport();
    RenderInspector();
    RenderConsole();
}

void EditorUI::RenderDockspace() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags hostFlags =
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("EditorDockspaceHost", nullptr, hostFlags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspaceID = ImGui::GetID("EditorDockspace");
    ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    // Строим layout только один раз, при первом запуске (или если .ini нет).
    // После этого пользователь сам управляет размерами окон, и эти размеры
    // сохраняются в imgui.ini между запусками.
    static bool layoutBuilt = false;
    if (!layoutBuilt) {
        layoutBuilt = true;

        ImGui::DockBuilderRemoveNode(dockspaceID);
        ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceID, viewport->WorkSize);

        ImGuiID dockMain = dockspaceID;

        // Сначала отрезаем Console на всю ширину снизу — поэтому увеличение
        // её высоты сжимает по высоте ВСЁ, что осталось сверху (Scene Objects,
        // Viewport, Inspector) одновременно, как ты и хотел.
        ImGuiID dockBottom = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.28f, nullptr, &dockMain);

        // Затем из оставшейся верхней полосы отрезаем левую (Scene Objects)
        // и правую (Inspector) колонки. То, что останется в центре — Viewport,
        // он и будет "тянуться" при изменении соседних панелей.
        ImGuiID dockLeft = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.18f, nullptr, &dockMain);
        ImGuiID dockRight = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.22f, nullptr, &dockMain);

        // У нас всегда ровно одно окно на панель — полоса вкладок (и стрелка,
        // которая её возвращает) тут никогда не нужна, убираем полностью.
        for (ImGuiID id : { dockLeft, dockRight, dockBottom, dockMain }) {
            if (ImGuiDockNode* node = ImGui::DockBuilderGetNode(id))
                node->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
        }

        ImGui::DockBuilderDockWindow("Scene Objects", dockLeft);
        ImGui::DockBuilderDockWindow("Inspector", dockRight);
        ImGui::DockBuilderDockWindow("Console", dockBottom);
        ImGui::DockBuilderDockWindow("Viewport", dockMain);

        ImGui::DockBuilderFinish(dockspaceID);
    }

    ImGui::End();
}

void EditorUI::RenderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
                LogInfo("Creating new scene...");
                m_gameObjects.clear();
                m_objectCounter = 0;
                AddGameObject("Main Camera");
                LogSuccess("New scene created");
            }
            if (ImGui::MenuItem("Open Scene", "Ctrl+O")) {
                const std::string path = "Scenes/MainScene.escn";
                std::vector<GameObject> loaded;
                if (SceneSerializer::Load(path, loaded)) {
                    m_gameObjects = std::move(loaded);
                    m_selectedObjectIndex = -1;
                    LogSuccess("Scene loaded: " + path);
                }
                else {
                    LogError("Failed to load scene: " + path);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
                const std::string path = "Scenes/MainScene.escn";
                std::filesystem::create_directories("Scenes");
                if (SceneSerializer::Save(path, m_gameObjects)) {
                    AssetDatabase::GetOrCreateGuid(path, "Scene");
                    LogSuccess("Scene saved: " + path);
                }
                else {
                    LogError("Failed to save scene: " + path);
                }
            }
            if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S")) {
                LogInfo("Save scene as... (not implemented)");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                glfwSetWindowShouldClose(m_window, true);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
                LogInfo("Undo (not implemented)");
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
                LogInfo("Redo (not implemented)");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "Ctrl+X")) {}
            if (ImGui::MenuItem("Copy", "Ctrl+C")) {}
            if (ImGui::MenuItem("Paste", "Ctrl+V")) {}
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Scene Objects", nullptr, &m_showHierarchy);
            ImGui::MenuItem("Inspector", nullptr, &m_showInspector);
            ImGui::MenuItem("Console", nullptr, &m_showConsole);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("GameObject")) {
            if (ImGui::MenuItem("Create Empty", "Ctrl+Shift+N")) {
                AddGameObject("GameObject");
            }
            if (ImGui::MenuItem("Camera")) {
                AddGameObject("Camera");
            }
            if (ImGui::MenuItem("Light")) {
                AddGameObject("Light");
            }
            if (ImGui::MenuItem("Cube")) {
                AddGameObject("Cube");
            }
            if (ImGui::MenuItem("Sphere")) {
                AddGameObject("Sphere");
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) {
                LogInfo("Engine v0.1 - Built with C++ and ImGui");
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void EditorUI::RenderHierarchy() {
    if (!m_showHierarchy) return;

    ImGui::Begin("Scene Objects", &m_showHierarchy, ImGuiWindowFlags_NoCollapse);

    ImGui::TextDisabled("%zu objects in scene", m_gameObjects.size());
    ImGui::Separator();

    for (int i = 0; i < (int)m_gameObjects.size(); i++) {
        RenderGameObjectNode(m_gameObjects[i], i);
    }

    ImGui::Separator();

    if (ImGui::Button("+ Add Object")) {
        AddGameObject("GameObject");
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete Selected")) {
        if (m_selectedObjectIndex >= 0 && m_selectedObjectIndex < (int)m_gameObjects.size()) {
            std::string name = m_gameObjects[m_selectedObjectIndex].name;
            m_gameObjects.erase(m_gameObjects.begin() + m_selectedObjectIndex);
            m_selectedObjectIndex = -1;
            LogInfo("Deleted: " + name);
        }
    }

    ImGui::End();
}

void EditorUI::RenderGameObjectNode(GameObject& obj, int index) {
    bool isSelected = (m_selectedObjectIndex == index);

    if (ImGui::Selectable(obj.name.c_str(), isSelected)) {
        SelectGameObject(index);
        LogInfo("Selected: " + obj.name);
    }

    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Duplicate")) {
            GameObject newObj = obj;
            newObj.name = obj.name + "_copy";
            m_gameObjects.push_back(newObj);
            LogInfo("Duplicated: " + obj.name);
        }
        if (ImGui::MenuItem("Delete")) {
            m_gameObjects.erase(m_gameObjects.begin() + index);
            if (m_selectedObjectIndex == index) {
                m_selectedObjectIndex = -1;
            }
            LogInfo("Deleted: " + obj.name);
        }
        ImGui::EndPopup();
    }
}

void EditorUI::RenderViewport() {
    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoCollapse);

    ImVec2 viewportSize = ImGui::GetContentRegionAvail();
    m_viewportWidth = viewportSize.x;
    m_viewportHeight = viewportSize.y;

    ImVec2 imagePos = ImGui::GetCursorPos();

    float aspect = (viewportSize.y > 0) ? viewportSize.x / viewportSize.y : 1.0f;
    glm::mat4 view = m_camera.GetViewMatrix();
    glm::mat4 projection = m_camera.GetProjectionMatrix(aspect);

    if (m_sceneRenderer && viewportSize.x > 0 && viewportSize.y > 0) {
        uint32_t textureID = m_sceneRenderer->Render(
            m_gameObjects, m_selectedObjectIndex,
            (int)viewportSize.x, (int)viewportSize.y,
            view, projection);

        if (textureID != 0) {
            // UV перевёрнут по Y: у OpenGL текстуры начало координат снизу,
            // у ImGui — сверху.
            ImGui::Image((ImTextureID)(intptr_t)textureID, viewportSize, ImVec2(0, 1), ImVec2(1, 0));
        }
    }

    // Невидимая кнопка ровно над картинкой — через неё ловим ввод мыши для
    // камеры. Явно указываем левую и среднюю кнопки: по умолчанию
    // InvisibleButton считает "активным" элемент только при левом клике,
    // без этого флага драг средней кнопкой не отслеживался бы вообще.
    ImGui::SetCursorPos(imagePos);
    if (viewportSize.x > 0 && viewportSize.y > 0) {
        ImGui::InvisibleButton("ViewportInteraction", viewportSize,
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);

        if (ImGui::IsItemHovered()) {
            float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f) m_camera.Zoom(wheel);
        }

        if (ImGui::IsItemActivated()) {
            m_viewportDragAccum = 0.0f;
        }

        if (ImGui::IsItemActive()) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            m_viewportDragAccum += std::sqrt(delta.x * delta.x + delta.y * delta.y);

            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                m_camera.OrbitDrag(delta.x, delta.y);
            }
            else if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
                m_camera.Pan(delta.x, delta.y);
            }
        }

        // Если кнопку отпустили почти без сдвига мыши — это был клик, а не
        // драг камеры, поэтому делаем picking. Порог в пикселях защищает от
        // случайного дрожания руки во время обычного клика.
        if (ImGui::IsItemDeactivated() && m_viewportDragAccum < 4.0f) {
            ImVec2 itemMin = ImGui::GetItemRectMin();
            ImVec2 mousePos = ImGui::GetIO().MousePos;
            float localX = mousePos.x - itemMin.x;
            float localY = mousePos.y - itemMin.y;

            float ndcX = (2.0f * localX) / viewportSize.x - 1.0f;
            float ndcY = 1.0f - (2.0f * localY) / viewportSize.y;

            int hitIndex = PickObject(m_gameObjects, view, projection, ndcX, ndcY);
            if (hitIndex >= 0) {
                SelectGameObject(hitIndex);
                LogInfo("Selected: " + m_gameObjects[hitIndex].name);
            }
            else {
                m_selectedObjectIndex = -1;  // клик в пустоту снимает выделение
            }
        }
    }

    ImGui::SetCursorPos(ImVec2(8, 8));
    if (m_selectedObjectIndex >= 0 && m_selectedObjectIndex < (int)m_gameObjects.size()) {
        ImGui::TextColored(ImVec4(0.45f, 0.70f, 1.00f, 1.0f),
            "%zu objects | Selected: %s | %.0f FPS",
            m_gameObjects.size(),
            m_gameObjects[m_selectedObjectIndex].name.c_str(),
            ImGui::GetIO().Framerate);
    }
    else {
        ImGui::TextColored(ImVec4(0.45f, 0.70f, 1.00f, 1.0f),
            "%zu objects | %.0f FPS",
            m_gameObjects.size(),
            ImGui::GetIO().Framerate);
    }

    ImGui::End();
}

void EditorUI::RenderInspector() {
    if (!m_showInspector) return;

    ImGui::Begin("Inspector", &m_showInspector, ImGuiWindowFlags_NoCollapse);

    GameObject* selected = GetSelectedObject();
    if (selected) {
        ImGui::TextColored(ImVec4(0.45f, 0.70f, 1.00f, 1.0f), "Selected: %s", selected->name.c_str());
        ImGui::Separator();

        ImGui::Text("Transform");
        ImGui::Indent();
        RenderTransformEditor(selected);
        ImGui::Unindent();

        ImGui::Separator();
        ImGui::Text("Components");
        ImGui::Indent();
        ImGui::TextDisabled("No component system yet.");
        ImGui::TextDisabled("This panel will list real components");
        ImGui::TextDisabled("once the engine core exists.");
        ImGui::Unindent();
    }
    else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No object selected");
        ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "Select an object in the Scene Objects panel");
    }

    ImGui::End();
}

void EditorUI::RenderTransformEditor(GameObject* obj) {
    if (!obj) return;
    ImGui::DragFloat3("Position", obj->position, 0.1f);
    ImGui::DragFloat3("Rotation", obj->rotation, 0.1f);
    ImGui::DragFloat3("Scale", obj->scale, 0.1f, 0.01f, 100.0f);
}

void EditorUI::RenderConsole() {
    if (!m_showConsole) return;

    ImGui::Begin("Console", &m_showConsole, ImGuiWindowFlags_NoCollapse);

    ImGui::Checkbox("Auto-scroll", &m_autoScrollConsole);
    ImGui::SameLine();

    if (ImGui::Button("Clear")) {
        m_consoleMessages.clear();
        m_shouldAutoScroll = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("| %zu messages", m_consoleMessages.size());

    ImGui::Separator();

    ImGui::BeginChild("ConsoleMessages", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

    for (const auto& msg : m_consoleMessages) {
        ImVec4 color;
        const char* tag;
        switch (msg.type) {
        case ConsoleMessage::Info:    color = ImVec4(0.5f, 0.8f, 1.0f, 1.0f); tag = "INFO "; break;
        case ConsoleMessage::Warning: color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f); tag = "WARN "; break;
        case ConsoleMessage::Error:   color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f); tag = "ERROR"; break;
        case ConsoleMessage::Success: color = ImVec4(0.3f, 0.8f, 0.3f, 1.0f); tag = "OK   "; break;
        }

        ImGui::TextColored(color, "[%s] %s  %s", msg.timestamp.c_str(), tag, msg.text.c_str());
    }

    if (m_shouldAutoScroll && m_autoScrollConsole && !m_consoleMessages.empty()) {
        ImGui::SetScrollHereY(1.0f);
        m_shouldAutoScroll = false;
    }

    ImGui::EndChild();

    ImGui::End();
}

void EditorUI::AddGameObject(const std::string& name) {
    GameObject obj;
    obj.name = name + "_" + std::to_string(m_objectCounter++);
    m_gameObjects.push_back(obj);
    LogInfo("Created: " + obj.name);
}

void EditorUI::SelectGameObject(int index) {
    if (index >= 0 && index < (int)m_gameObjects.size()) {
        m_selectedObjectIndex = index;
    }
}

GameObject* EditorUI::GetSelectedObject() {
    if (m_selectedObjectIndex >= 0 && m_selectedObjectIndex < (int)m_gameObjects.size()) {
        return &m_gameObjects[m_selectedObjectIndex];
    }
    return nullptr;
}

void EditorUI::SetViewportSize(float width, float height) {
    m_viewportWidth = width;
    m_viewportHeight = height;
}

void EditorUI::LogInfo(const std::string& message) {
    AddConsoleMessage(message, ConsoleMessage::Info);
}

void EditorUI::LogWarning(const std::string& message) {
    AddConsoleMessage(message, ConsoleMessage::Warning);
}

void EditorUI::LogError(const std::string& message) {
    AddConsoleMessage(message, ConsoleMessage::Error);
}

void EditorUI::LogSuccess(const std::string& message) {
    AddConsoleMessage(message, ConsoleMessage::Success);
}

void EditorUI::AddConsoleMessage(const std::string& message, ConsoleMessage::Type type) {
    ConsoleMessage msg;
    msg.type = type;
    msg.text = message;

    time_t t = std::time(nullptr);
    struct tm tm;
    localtime_s(&tm, &t);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%H:%M:%S");
    msg.timestamp = oss.str();

    m_consoleMessages.push_back(msg);
    m_shouldAutoScroll = true;

    if (m_consoleMessages.size() > 1000) {
        m_consoleMessages.erase(m_consoleMessages.begin());
    }
}