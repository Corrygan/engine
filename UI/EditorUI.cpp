#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "EditorUI.h"
#include "IconsFontAwesome6.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include "glfw3native.h"
#include <windows.h>
#include <shellapi.h>
#include "imgui_internal.h"
#include "../Assets/SceneSerializer.h"
#include "../Assets/AssetDatabase.h"
#include "../Scripting/LuaScripting.h"
#include "../Assets/ModelImporter.h"
#include "../Renderer/SceneRenderer.h"
#include "../Renderer/ModelMesh.h"
#include "../Renderer/Material.h"
#include "../Renderer/MaterialManager.h"
#include "../Renderer/MaterialPreviewRenderer.h"
#include "../Renderer/TextureManager.h"
#include "../Renderer/Picking.h"
#include "../Jobs/JobSystem.h"
#include "../Physics/PhysicsWorld.h"
#include "../Core/Branding.h"
#include "../Assets/ProjectFile.h"
#include "EditorStyle.h"
#include "ImGuizmo.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <unordered_set>
#include <cstdint>
#include <cmath>
#include <commdlg.h>

namespace {
    bool FieldSpinner(const char* id, float* v, float step,
                      float vmin, float vmax,
                      const char* fmt = "%.3f", float width = -1.0f)
    {
        if (width < 0.0f) width = ImGui::GetContentRegionAvail().x;
        ImGui::PushID(id);
        ImGui::BeginGroup();

        float h      = ImGui::GetFrameHeight();
        float half   = h * 0.5f;
        float arrowW = h;
        float inputW = width - arrowW - 1.0f;
        float r      = ImGui::GetStyle().FrameRounding;
        bool  changed = false;

        ImVec2 pos    = ImGui::GetCursorScreenPos();
        ImVec2 posMax = {pos.x + width, pos.y + h};
        ImDrawList* dl = ImGui::GetWindowDrawList();

        dl->AddRectFilled(pos, posMax, ImGui::GetColorU32(ImGuiCol_FrameBg), r);

        ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,  ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive,   ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_Border,          ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_BorderShadow,    ImVec4(0,0,0,0));
        ImGui::SetNextItemWidth(inputW);
        if (ImGui::InputFloat("##fi", v, 0, 0, fmt)) {
            *v = std::clamp(*v, vmin, vmax);
            changed = true;
        }
        bool inHov = ImGui::IsItemHovered();
        bool inAct = ImGui::IsItemActive();
        ImGui::PopStyleColor(5);

        ImU32 brdCol = (inHov || inAct) ? IM_COL32(160,165,185,130) : IM_COL32(255,255,255,45);
        dl->AddRect(pos, posMax, brdCol, r, 0, 1.0f);
        dl->AddLine({pos.x + inputW + 1, pos.y + 4},
                    {pos.x + inputW + 1, posMax.y - 4}, IM_COL32(255,255,255,28));

        ImGui::SetCursorScreenPos({pos.x + inputW + 1.0f, pos.y});
        ImVec2 btnOrigin = {pos.x + inputW + 1.0f, pos.y};

        auto applyHeld = [&](float dir) {
            float held = ImGui::GetIO().MouseDownDuration[0];
            float mult = held > 2.0f ? 20.0f : held > 1.0f ? 5.0f : held > 0.3f ? 2.0f : 1.0f;
            *v = std::clamp(*v + dir * step * mult, vmin, vmax);
            changed = true;
        };

        ImGui::PushButtonRepeat(true);

        ImGui::SetCursorScreenPos(btnOrigin);
        bool upClicked = ImGui::InvisibleButton("##up", ImVec2(arrowW, half));
        bool upHov     = ImGui::IsItemHovered();
        if (upClicked) applyHeld(+1.0f);

        ImGui::SetCursorScreenPos({btnOrigin.x, btnOrigin.y + half});
        bool dnClicked = ImGui::InvisibleButton("##dn", ImVec2(arrowW, half));
        bool dnHov     = ImGui::IsItemHovered();
        if (dnClicked) applyHeld(-1.0f);

        ImGui::PopButtonRepeat();

        float cx   = std::floor(btnOrigin.x + arrowW * 0.5f);
        float ts   = 3.0f;
        float upCy = std::floor(btnOrigin.y + half * 0.5f);
        float dnCy = std::floor(btnOrigin.y + half * 1.5f);
        ImU32 upAc = upHov ? IM_COL32(215,215,225,255) : IM_COL32(138,138,152,190);
        ImU32 dnAc = dnHov ? IM_COL32(215,215,225,255) : IM_COL32(138,138,152,190);
        dl->AddTriangleFilled({cx-ts,      upCy+ts}, {cx+ts,      upCy+ts}, {cx, upCy-ts+0.6f}, upAc);
        dl->AddTriangleFilled({cx-ts-0.5f, dnCy-ts}, {cx+ts+0.5f, dnCy-ts}, {cx, dnCy+ts}, dnAc);

        ImGui::EndGroup();
        ImGui::PopID();
        return changed;
    }

    bool ColorSwatchEdit(const char* id, float color[], bool hasAlpha = true)
    {
        ImGui::PushID(id);
        float h = ImGui::GetFrameHeight();
        float w = h * 3.0f;
        ImVec2 pos = ImGui::GetCursorScreenPos();
        bool changed = false;

        ImGui::InvisibleButton("##csw", ImVec2(w, h));
        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) ImGui::OpenPopup("##colorpop");

        if (ImGui::BeginPopup("##colorpop")) {
            ImGuiColorEditFlags f = ImGuiColorEditFlags_NoSidePreview
                                  | ImGuiColorEditFlags_PickerHueWheel;
            if (hasAlpha) f |= ImGuiColorEditFlags_AlphaBar;
            changed = ImGui::ColorPicker4("##pk", color, f);
            ImGui::EndPopup();
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 pMax(pos.x + w, pos.y + h);

        // Checkerboard for alpha
        if (hasAlpha && color[3] < 0.99f) {
            float sz = h * 0.5f;
            int cols = (int)(w / sz) + 1;
            for (int row = 0; row < 2; ++row)
                for (int col = 0; col < cols; ++col) {
                    ImU32 bc = ((row+col)%2==0) ? IM_COL32(140,140,140,255) : IM_COL32(90,90,90,255);
                    ImVec2 cMin = {pos.x+col*sz, pos.y+row*sz};
                    ImVec2 cMax = {std::min(pos.x+(col+1)*sz, pMax.x), pos.y+(row+1)*sz};
                    dl->AddRectFilled(cMin, cMax, bc);
                }
        }

        ImU32 col32 = IM_COL32((int)(color[0]*255),(int)(color[1]*255),(int)(color[2]*255),
                                hasAlpha ? (int)(color[3]*255) : 255);
        dl->AddRectFilled(pos, pMax, col32, 4.0f);
        dl->AddRect(pos, pMax,
            hov ? IM_COL32(192,54,76,200) : IM_COL32(255,255,255,55), 4.0f, 0, 1.2f);

        // Hex code in tooltip, not inside the small swatch
        if (hov) {
            char hex[12];
            snprintf(hex, sizeof(hex), "#%02X%02X%02X%02X",
                (int)(color[0]*255),(int)(color[1]*255),(int)(color[2]*255),
                hasAlpha ? (int)(color[3]*255) : 255);
            ImGui::SetTooltip("%s  вЂ”  click to edit", hex);
        }

        ImGui::PopID();
        return changed;
    }

    bool FillSlider(const char* id, const char* label, float* v,
                    float vmin, float vmax, const char* fmt = "%.2f")
    {
        ImGui::PushID(id);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        float  w   = ImGui::GetContentRegionAvail().x;
        float  h   = ImGui::GetFrameHeight();
        bool   changed = false;

        ImGui::InvisibleButton("##fs", ImVec2(w, h));
        bool hovered = ImGui::IsItemHovered();
        bool active  = ImGui::IsItemActive();

        if (active) {
            float t = std::clamp((ImGui::GetIO().MousePos.x - pos.x) / w, 0.0f, 1.0f);
            float nv = vmin + t * (vmax - vmin);
            if (nv != *v) { *v = nv; changed = true; }
        }


        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 pMax(pos.x + w, pos.y + h);
        float t = std::clamp((*v - vmin) / (vmax - vmin), 0.0f, 1.0f);

        dl->AddRectFilled(pos, pMax, IM_COL32(28, 28, 35, 255), 4.0f);
        if (t > 0.005f)
            dl->AddRectFilled(pos, {pos.x + w * t, pMax.y},
                active ? IM_COL32(100, 105, 125, 255) : IM_COL32(85, 90, 110, 200), 4.0f);
        dl->AddRect(pos, pMax,
            (hovered || active) ? IM_COL32(160, 165, 185, 90) : IM_COL32(255, 255, 255, 18),
            4.0f, 0, 1.0f);

        char buf[32]; snprintf(buf, sizeof(buf), fmt, *v);
        ImVec2 ts = ImGui::CalcTextSize(buf);
        dl->AddText({pos.x + (w - ts.x) * 0.5f, pos.y + (h - ts.y) * 0.5f},
                    IM_COL32(215, 215, 222, 255), buf);

        if (hovered) ImGui::SetTooltip("%s", label);
        ImGui::PopID();
        return changed;
    }

    bool IconButton(const char* icon, const char* id, ImVec2 sz,
                    ImU32 iconCol = 0, float rounding = 4.0f)
    {
        ImGui::PushID(id);
        bool clicked = ImGui::InvisibleButton("##ib", sz);
        bool hovered = ImGui::IsItemHovered();
        bool active  = ImGui::IsItemActive();
        ImVec2 rMin  = ImGui::GetItemRectMin();
        ImVec2 rMax  = ImGui::GetItemRectMax();

        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (active)
            dl->AddRectFilled(rMin, rMax, ImGui::GetColorU32(ImGuiCol_ButtonActive),  rounding);
        else if (hovered)
            dl->AddRectFilled(rMin, rMax, ImGui::GetColorU32(ImGuiCol_ButtonHovered), rounding);

        if (iconCol == 0)
            iconCol = ImGui::GetColorU32(ImGuiCol_Text);

        ImVec2 ts  = ImGui::CalcTextSize(icon);
        ImVec2 tp  = { rMin.x + (sz.x - ts.x) * 0.5f,
                       rMin.y + (sz.y - ts.y) * 0.5f };
        dl->AddText(tp, iconCol, icon);

        ImGui::PopID();
        return clicked;
    }

    std::string OpenTextureFileDialog() {
        OPENFILENAMEA ofn{};
        char fileName[MAX_PATH] = "";
        ofn.lStructSize  = sizeof(ofn);
        ofn.lpstrFile    = fileName;
        ofn.nMaxFile     = MAX_PATH;
        ofn.lpstrFilter  = "Images\0*.png;*.jpg;*.jpeg;*.bmp;*.tga\0All Files\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
        return GetOpenFileNameA(&ofn) ? std::string(fileName) : "";
    }

    std::string OpenModelFileDialog() {
        OPENFILENAMEA ofn{};
        char fileName[MAX_PATH] = "";
        ofn.lStructSize  = sizeof(ofn);
        ofn.lpstrFile    = fileName;
        ofn.nMaxFile     = MAX_PATH;
        ofn.lpstrFilter  = "3D Models\0*.obj;*.fbx;*.gltf;*.glb\0All Files\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
        return GetOpenFileNameA(&ofn) ? std::string(fileName) : "";
    }

    // Copy every component (except Name, set at create time) from one entity to
    // another — possibly across registries. Used for Duplicate and the play-mode
    // snapshot. Both src and dst already have Name + Transform (via Scene::Create).
    void CopyEntity(const entt::registry& s, entt::entity se,
                    entt::registry& d, entt::entity de) {
        d.get<TransformComponent>(de) = s.get<TransformComponent>(se);
        if (auto* c = s.try_get<MeshComponent>(se))     d.emplace_or_replace<MeshComponent>(de, *c);
        if (auto* c = s.try_get<MaterialComponent>(se)) d.emplace_or_replace<MaterialComponent>(de, *c);
        if (auto* c = s.try_get<LightComponent>(se))    d.emplace_or_replace<LightComponent>(de, *c);
        if (auto* c = s.try_get<CameraComponent>(se))   d.emplace_or_replace<CameraComponent>(de, *c);
        if (auto* c = s.try_get<RotatorComponent>(se))  d.emplace_or_replace<RotatorComponent>(de, *c);
        if (auto* c = s.try_get<FloaterComponent>(se))  d.emplace_or_replace<FloaterComponent>(de, *c);
        if (auto* c = s.try_get<ScriptComponent>(se))   d.emplace_or_replace<ScriptComponent>(de, *c);
        if (auto* c = s.try_get<ColliderComponent>(se))  d.emplace_or_replace<ColliderComponent>(de, *c);
        if (auto* c = s.try_get<RigidBodyComponent>(se)) d.emplace_or_replace<RigidBodyComponent>(de, *c);
        if (auto* c = s.try_get<CharacterControllerComponent>(se)) d.emplace_or_replace<CharacterControllerComponent>(de, *c);
    }

    // Write a (local) TRS matrix back into a TransformComponent, dropping any
    // shear/perspective. Used by the gizmo and reparenting.
    void SetTransformFromMatrix(TransformComponent& tr, const glm::mat4& m) {
        tr.position = glm::vec3(m[3]);
        glm::vec3 sc(glm::length(glm::vec3(m[0])),
                     glm::length(glm::vec3(m[1])),
                     glm::length(glm::vec3(m[2])));
        tr.scale = sc;
        if (sc.x > 1e-5f && sc.y > 1e-5f && sc.z > 1e-5f) {
            glm::mat3 rot(glm::vec3(m[0]) / sc.x,
                          glm::vec3(m[1]) / sc.y,
                          glm::vec3(m[2]) / sc.z);
            tr.rotation = glm::quat_cast(rot);
        }
    }

    // Deep-copy every entity (Name + all components) from src into dst, filling
    // srcToDst with the old->new id remap. Used for undo/redo snapshots. The
    // parent hierarchy is re-linked through the remap so parenting survives undo.
    void CloneScene(const Scene& src, Scene& dst,
                    std::unordered_map<entt::entity, entt::entity>& srcToDst) {
        dst.Clear();
        srcToDst.clear();
        const entt::registry& s = src.Reg();
        for (entt::entity e : s.view<NameComponent>()) {
            entt::entity n = dst.Create(s.get<NameComponent>(e).name);
            CopyEntity(s, e, dst.Reg(), n);
            srcToDst[e] = n;
        }
        // Re-link parents in dst id space (CopyEntity intentionally skips the link).
        entt::registry& d = dst.Reg();
        for (auto [se, de] : srcToDst) {
            if (const auto* pc = s.try_get<ParentComponent>(se)) {
                auto it = srcToDst.find(pc->parent);
                if (it != srcToDst.end())
                    d.emplace<ParentComponent>(de, ParentComponent{ it->second });
            }
        }
    }

    std::string OpenScriptFileDialog() {
        OPENFILENAMEA ofn{};
        char fileName[MAX_PATH] = "";
        ofn.lStructSize  = sizeof(ofn);
        ofn.lpstrFile    = fileName;
        ofn.nMaxFile     = MAX_PATH;
        ofn.lpstrFilter  = "Lua Scripts\0*.lua\0All Files\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
        return GetOpenFileNameA(&ofn) ? std::string(fileName) : "";
    }

    std::string OpenSceneFileDialog() {
        OPENFILENAMEA ofn{};
        char fileName[MAX_PATH] = "";
        ofn.lStructSize  = sizeof(ofn);
        ofn.lpstrFile    = fileName;
        ofn.nMaxFile     = MAX_PATH;
        ofn.lpstrFilter  = "Force Scene\0*.fcscn;*.escn\0All Files\0*.*\0";
        ofn.lpstrDefExt  = "fcscn";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
        return GetOpenFileNameA(&ofn) ? std::string(fileName) : "";
    }

    std::string SaveSceneFileDialog() {
        OPENFILENAMEA ofn{};
        char fileName[MAX_PATH] = "";
        ofn.lStructSize  = sizeof(ofn);
        ofn.lpstrFile    = fileName;
        ofn.nMaxFile     = MAX_PATH;
        ofn.lpstrFilter  = "Force Scene\0*.fcscn\0All Files\0*.*\0";
        ofn.lpstrDefExt  = "fcscn";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
        return GetSaveFileNameA(&ofn) ? std::string(fileName) : "";
    }

    std::string OpenProjectFileDialog() {
        OPENFILENAMEA ofn{};
        char fileName[MAX_PATH] = "";
        ofn.lStructSize  = sizeof(ofn);
        ofn.lpstrFile    = fileName;
        ofn.nMaxFile     = MAX_PATH;
        ofn.lpstrFilter  = "Force Project\0*.fcproj\0All Files\0*.*\0";
        ofn.lpstrDefExt  = "fcproj";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
        return GetOpenFileNameA(&ofn) ? std::string(fileName) : "";
    }

    std::string SaveProjectFileDialog() {
        OPENFILENAMEA ofn{};
        char fileName[MAX_PATH] = "";
        ofn.lStructSize  = sizeof(ofn);
        ofn.lpstrFile    = fileName;
        ofn.nMaxFile     = MAX_PATH;
        ofn.lpstrFilter  = "Force Project\0*.fcproj\0All Files\0*.*\0";
        ofn.lpstrDefExt  = "fcproj";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
        return GetSaveFileNameA(&ofn) ? std::string(fileName) : "";
    }

    std::string SavePrefabFileDialog(const std::string& initialDir, const std::string& defName) {
        OPENFILENAMEA ofn{};
        char fileName[MAX_PATH] = "";
        if (!defName.empty()) strncpy_s(fileName, sizeof(fileName), defName.c_str(), _TRUNCATE);
        ofn.lStructSize  = sizeof(ofn);
        ofn.lpstrFile    = fileName;
        ofn.nMaxFile     = MAX_PATH;
        ofn.lpstrFilter  = "Force Prefab\0*.fcprefab\0All Files\0*.*\0";
        ofn.lpstrDefExt  = "fcprefab";
        ofn.nFilterIndex = 1;
        if (!initialDir.empty()) ofn.lpstrInitialDir = initialDir.c_str();
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
        return GetSaveFileNameA(&ofn) ? std::string(fileName) : "";
    }
}

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

    m_sceneRenderer   = new SceneRenderer();
    m_previewRenderer = new MaterialPreviewRenderer();
    m_nodeEditor      = new MaterialNodeEditor();
    m_lua             = new LuaScripting();
    m_lua->SetLogCallback([this](const std::string& msg) { LogInfo(msg); });
    m_physics         = new PhysicsWorld();

    jobs::JobSystem::Get().Initialize();
    LogInfo("Job system: " + std::to_string(jobs::JobSystem::Get().WorkerCount())
            + " worker threads");

    NewDocument("Untitled");   // always have one active scene document

    // When a material is renamed in the node editor, re-point objects + UI.
    m_nodeEditor->SetRenameHandler([this](const std::string& oldP, const std::string& newP) {
        for (auto [e, mc] : m_scene.Reg().view<MaterialComponent>().each())
            if (mc.path == oldP) mc.path = newP;
        if (m_assetBrowserSelected == oldP) m_assetBrowserSelected = newP;
        m_compiledGraphs.erase(oldP);          // re-precompile under the new path
        m_assetBrowserDirty = true;
        MarkDirty();
        LogInfo("Material renamed: " + std::filesystem::path(newP).stem().string());
    });

    glfwSetWindowIconifyCallback(m_window, [](GLFWwindow*, int iconified) {
        if (!iconified) {
            ImGuiIO& io = ImGui::GetIO();
            for (int i = 0; i < 5; ++i)
                io.AddMouseButtonEvent(i, false);
            ImGui::ClearActiveID();
        }
    });

    ApplyEngineStyle();

    ImGuizmo::Style& gs = ImGuizmo::GetStyle();
    gs.Colors[ImGuizmo::DIRECTION_X]      = ImVec4(0.86f, 0.24f, 0.24f, 1.00f);
    gs.Colors[ImGuizmo::DIRECTION_Y]      = ImVec4(0.24f, 0.76f, 0.24f, 1.00f);
    gs.Colors[ImGuizmo::DIRECTION_Z]      = ImVec4(0.24f, 0.46f, 0.90f, 1.00f);
    gs.Colors[ImGuizmo::PLANE_X]          = ImVec4(0.86f, 0.24f, 0.24f, 0.22f);
    gs.Colors[ImGuizmo::PLANE_Y]          = ImVec4(0.24f, 0.76f, 0.24f, 0.22f);
    gs.Colors[ImGuizmo::PLANE_Z]          = ImVec4(0.24f, 0.46f, 0.90f, 0.22f);
    gs.Colors[ImGuizmo::SELECTION]        = ImVec4(1.00f, 1.00f, 1.00f, 0.88f);
    gs.Colors[ImGuizmo::INACTIVE]         = ImVec4(0.50f, 0.50f, 0.50f, 0.50f);
    gs.Colors[ImGuizmo::TRANSLATION_LINE] = ImVec4(0.70f, 0.70f, 0.70f, 0.70f);

    return true;
}

void EditorUI::Shutdown() {
    jobs::JobSystem::Get().Shutdown();   // join workers before tearing subsystems down
    delete m_physics;         m_physics         = nullptr;
    delete m_lua;             m_lua             = nullptr;
    delete m_nodeEditor;      m_nodeEditor      = nullptr;
    delete m_previewRenderer; m_previewRenderer = nullptr;
    delete m_sceneRenderer;   m_sceneRenderer   = nullptr;
}

void EditorUI::Render() {
    if (!m_window) return;

    static bool s_wasIconified = false;
    const bool  isIconified    = glfwGetWindowAttrib(m_window, GLFW_ICONIFIED) != 0;
    if (s_wasIconified && !isIconified) {
        ImGuiIO& io = ImGui::GetIO();
        for (int i = 0; i < 5; ++i)
            io.AddMouseButtonEvent(i, false);
        ImGui::ClearActiveID();
    }
    s_wasIconified = isIconified;

    // Run results marshaled back from worker threads (e.g. finished async imports).
    jobs::JobSystem::Get().RunMainThreadTasks();

    if (glfwWindowShouldClose(m_window) && m_sceneDirty) {
        glfwSetWindowShouldClose(m_window, 0);
        m_showUnsavedChangesDialog = true;
    }

    UpdatePlayMode();

    RenderDockspace();
    RenderHierarchy();
    RenderViewport();
    RenderInspector();
    RenderConsole();
    RenderAssetBrowser();

    if (m_nodeEditor) m_nodeEditor->Render();
    RenderUnsavedChangesDialog();
    RenderCloseSceneDialog();

    // Undo/redo: keyboard shortcuts + per-gesture snapshot coalescing. Done at
    // the end of the frame so ImGui's interaction state reflects this frame.
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl && !io.WantTextInput) {
            if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) { if (io.KeyShift) Redo(); else Undo(); }
            if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) Redo();
        }
    }
    UpdateUndoCoalescing();
}

void EditorUI::RenderTitleBar() {
    constexpr float kTitleH   = 32.0f;
    constexpr float kBtnW     = 46.0f;
    constexpr float kIconSize = 10.0f;

    ImGuiViewport* vp  = ImGui::GetMainViewport();
    ImVec2 wPos = vp->Pos;
    float  wW   = vp->Size.x;

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    dl->AddRectFilled(
        wPos,
        ImVec2(wPos.x + wW, wPos.y + kTitleH),
        IM_COL32(24, 24, 24, 255));
    dl->AddLine(
        ImVec2(wPos.x, wPos.y + kTitleH - 1),
        ImVec2(wPos.x + wW, wPos.y + kTitleH - 1),
        IM_COL32(255, 255, 255, 20));

    float textY = wPos.y + (kTitleH - ImGui::GetTextLineHeight()) * 0.5f;
    {
        // Brand mark: branding/icon.png if present, else a crimson placeholder.
        float  cy = wPos.y + kTitleH * 0.5f;
        ImVec2 p0(wPos.x + 12.0f, cy - 8.0f), p1(wPos.x + 28.0f, cy + 8.0f);
        Texture* logo = TextureManager::GetOrLoad(branding::kIconPath);
        if (logo && logo->GetID())
            dl->AddImage((ImTextureID)(intptr_t)logo->GetID(), p0, p1, ImVec2(0, 1), ImVec2(1, 0));
        else
            dl->AddRectFilled(p0, p1, IM_COL32(branding::kAccentR, branding::kAccentG,
                                               branding::kAccentB, 255), 4.0f);
    }
    dl->AddText(ImVec2(wPos.x + 32.0f, textY),
        IM_COL32(210, 210, 220, 255), branding::kAppName);

    {
        SceneDoc& doc = ActiveDoc();
        std::string label = doc.name.empty() ? "Untitled" : doc.name;
        if (doc.dirty) label = "\xe2\x80\xa2 " + label;
        if (m_docs.size() > 1) label += "   (" + std::to_string(m_docs.size()) + " scenes)";
        ImVec2 tsz = ImGui::CalcTextSize(label.c_str());
        dl->AddText(
            ImVec2(wPos.x + (wW - tsz.x) * 0.5f, textY),
            IM_COL32(120, 120, 140, 255), label.c_str());
    }

    HWND hwnd = glfwGetWin32Window(m_window);
    POINT cur; GetCursorPos(&cur);
    RECT  rc;  GetWindowRect(hwnd, &rc);
    int fromR = rc.right - cur.x;
    int fromT = cur.y - rc.top;
    bool inBar   = fromT >= 0 && fromT < (int)kTitleH;
    bool hClose  = inBar && fromR > 0          && fromR <= (int)kBtnW;
    bool hMax    = inBar && fromR > (int)kBtnW && fromR <= (int)(kBtnW * 2);
    bool hMin    = inBar && fromR > (int)(kBtnW * 2) && fromR <= (int)(kBtnW * 3);

    BOOL isMax = IsZoomed(hwnd);

    auto drawClose = [&](ImVec2 c, ImU32 col) {
        float h = kIconSize * 0.38f;
        dl->AddLine({c.x-h,c.y-h},{c.x+h,c.y+h}, col, 1.4f);
        dl->AddLine({c.x+h,c.y-h},{c.x-h,c.y+h}, col, 1.4f);
    };
    auto drawMaximize = [&](ImVec2 c, ImU32 col) {
        float h = kIconSize * 0.38f;
        dl->AddRect({c.x-h,c.y-h},{c.x+h,c.y+h}, col, 0, 0, 1.4f);
    };
    auto drawRestore = [&](ImVec2 c, ImU32 col) {
        float h = kIconSize * 0.36f, o = 3.0f;
        dl->AddRect({c.x-h+o,c.y-h-o},{c.x+h+o,c.y+h-o}, col, 0, 0, 1.3f);
        dl->AddRectFilled({c.x-h,c.y-h},{c.x+h,c.y-h+2.0f}, IM_COL32(9,9,16,255));
        dl->AddRect({c.x-h,c.y-h},{c.x+h,c.y+h}, col, 0, 0, 1.3f);
    };
    auto drawMinimize = [&](ImVec2 c, ImU32 col) {
        float h = kIconSize * 0.38f;
        dl->AddLine({c.x-h, c.y+h*0.5f},{c.x+h, c.y+h*0.5f}, col, 1.4f);
    };

    float btnY  = wPos.y + kTitleH * 0.5f;
    float cX    = wPos.x + wW - kBtnW * 0.5f;
    float maxX  = cX - kBtnW;
    float minX  = maxX - kBtnW;

    if (hClose)
        dl->AddRectFilled({wPos.x+wW-kBtnW, wPos.y},{wPos.x+wW, wPos.y+kTitleH},
            IM_COL32(196, 43, 28, 255));
    drawClose({cX, btnY}, hClose ? IM_COL32(255,255,255,255) : IM_COL32(155,155,168,255));

    if (hMax)
        dl->AddRectFilled({wPos.x+wW-kBtnW*2, wPos.y},{wPos.x+wW-kBtnW, wPos.y+kTitleH},
            IM_COL32(42, 42, 60, 255));
    if (isMax) drawRestore({maxX, btnY}, hMax ? IM_COL32(220,220,230,255) : IM_COL32(155,155,168,255));
    else       drawMaximize({maxX, btnY}, hMax ? IM_COL32(220,220,230,255) : IM_COL32(155,155,168,255));

    if (hMin)
        dl->AddRectFilled({wPos.x+wW-kBtnW*3, wPos.y},{wPos.x+wW-kBtnW*2, wPos.y+kTitleH},
            IM_COL32(42, 42, 60, 255));
    drawMinimize({minX, btnY}, hMin ? IM_COL32(220,220,230,255) : IM_COL32(155,155,168,255));

    float wWLocal = ImGui::GetWindowSize().x;
    const bool isMaximized = IsZoomed(hwnd) != 0;

    if (!isMaximized) {
        static POINT s_dragCursor = {};
        static POINT s_dragWindow = {};
        ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
        ImGui::InvisibleButton("##titleDrag", ImVec2(wWLocal - kBtnW * 3, kTitleH));
        if (ImGui::IsItemActivated()) {
            GetCursorPos(&s_dragCursor);
            RECT wr; GetWindowRect(hwnd, &wr);
            s_dragWindow = { wr.left, wr.top };
        }
        if (ImGui::IsItemActive()) {
            POINT cur; GetCursorPos(&cur);
            SetWindowPos(hwnd, nullptr,
                s_dragWindow.x + (cur.x - s_dragCursor.x),
                s_dragWindow.y + (cur.y - s_dragCursor.y),
                0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }

    {
        ImVec2 mouse    = ImGui::GetIO().MousePos;
        ImVec2 winPos   = ImGui::GetWindowPos();
        float  localX   = mouse.x - winPos.x;
        float  localY   = mouse.y - winPos.y;
        float  fromRight = wWLocal - localX;

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            localY >= 0.0f && localY < kTitleH && fromRight > 0.0f)
        {
            if      (fromRight <= kBtnW)       RequestClose();
            else if (fromRight <= kBtnW * 2)   m_pendingMaximize = true;
            else if (fromRight <= kBtnW * 3)   m_pendingMinimize = true;
        }
    }
}

void EditorUI::RenderDockspace() {
    constexpr float kTitleH  = 32.0f;
    constexpr float kMenuH   = 28.0f;
    constexpr float kHeaderH = kTitleH + kMenuH;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags hostFlags =
        ImGuiWindowFlags_NoDocking      |
        ImGuiWindowFlags_NoTitleBar     | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize       | ImGuiWindowFlags_NoMove     |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoBackground   | ImGuiWindowFlags_NoScrollbar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("EditorDockspaceHost", nullptr, hostFlags);
    ImGui::PopStyleVar(3);

    RenderTitleBar();
    RenderMenuBar();

    ImGui::SetCursorPos(ImVec2(0.0f, kHeaderH));
    ImGuiID dockspaceID = ImGui::GetID("EditorDockspaceV3");
    ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    static bool layoutBuilt = false;
    if (!layoutBuilt) {
        layoutBuilt = true;

        ImVec2 dockSize = ImVec2(viewport->Size.x, viewport->Size.y - kHeaderH);
        ImGui::DockBuilderRemoveNode(dockspaceID);
        ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceID, dockSize);

        ImGuiID dockMain   = dockspaceID;
        ImGuiID dockBottom = ImGui::DockBuilderSplitNode(dockMain,   ImGuiDir_Down,  0.19f, nullptr, &dockMain);
        ImGuiID dockLeft   = ImGui::DockBuilderSplitNode(dockMain,   ImGuiDir_Left,  0.15f, nullptr, &dockMain);
        ImGuiID dockRight  = ImGui::DockBuilderSplitNode(dockMain,   ImGuiDir_Right, 0.17f, nullptr, &dockMain);

        ImGuiID dockBotRight = 0;
        ImGuiID dockBotLeft  = ImGui::DockBuilderSplitNode(dockBottom, ImGuiDir_Left, 0.62f,
                                                            nullptr, &dockBotRight);

        for (ImGuiID id : { dockLeft, dockRight, dockBotLeft, dockBotRight, dockMain })
            if (ImGuiDockNode* node = ImGui::DockBuilderGetNode(id))
                node->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;

        ImGui::DockBuilderDockWindow("Scene Objects", dockLeft);
        ImGui::DockBuilderDockWindow("Inspector",     dockRight);
        ImGui::DockBuilderDockWindow("Asset Browser", dockBotLeft);
        ImGui::DockBuilderDockWindow("Console",       dockBotRight);
        ImGui::DockBuilderDockWindow("Viewport",      dockMain);
        ImGui::DockBuilderFinish(dockspaceID);
    }

    ImGui::End();
}

void EditorUI::RenderMenuBar() {
    constexpr float kTitleH = 32.0f;
    constexpr float kMenuH  = 28.0f;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(
        ImVec2(vp->Pos.x, vp->Pos.y + kTitleH),
        ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + kTitleH + kMenuH),
        IM_COL32(37, 37, 38, 255));
    dl->AddLine(
        ImVec2(vp->Pos.x, vp->Pos.y + kTitleH + kMenuH - 1),
        ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + kTitleH + kMenuH - 1),
        IM_COL32(255, 255, 255, 18));

    float btnH    = ImGui::GetFrameHeight();
    float btnOffY = kTitleH + (kMenuH - btnH) * 0.5f;
    ImGui::SetCursorPos(ImVec2(6.0f, btnOffY));

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,  4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,   ImVec2(10.0f, 3.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding,  6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(6.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,    ImVec2(8.0f, 2.0f));
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1,1,1,0.08f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1,1,1,0.13f));

    ImVec2 popupPos;
    auto MenuBtn = [&](const char* label, const char* id, bool separator = true) {
        bool clicked = ImGui::Button(label);
        popupPos = ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y + 1.0f);
        if (clicked) ImGui::OpenPopup(id);
        if (separator) {
            float x  = ImGui::GetItemRectMax().x + 4.0f;
            float y1 = ImGui::GetItemRectMin().y + 5.0f;
            float y2 = ImGui::GetItemRectMax().y - 5.0f;
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(x, y1), ImVec2(x, y2), IM_COL32(255, 255, 255, 35), 1.0f);
            ImGui::SameLine(0, 9);
        }
    };

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 4.0f));

    // Draw shortcut at a fixed distance from the right edge of the item rect.
    auto DrawShortcut = [&](const char* sc) {
        if (!sc || !*sc) return;
        ImVec2 rMin = ImGui::GetItemRectMin();
        ImVec2 rMax = ImGui::GetItemRectMax();
        ImVec2 sz   = ImGui::CalcTextSize(sc);
        ImGui::GetWindowDrawList()->AddText(
            {rMax.x - sz.x - 4.0f, rMin.y + (rMax.y - rMin.y - sz.y) * 0.5f},
            ImGui::GetColorU32(ImGuiCol_TextDisabled), sc);
    };

    // Pass shortcut to MenuItem so ImGui sizes the popup correctly,
    // but render it invisible (TextDisabled = transparent) then draw manually.
    auto MItem = [&](const char* label, const char* sc, bool* checked = nullptr) -> bool {
        ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0, 0, 0, 0));
        bool r = checked ? ImGui::MenuItem(label, sc, checked)
                         : ImGui::MenuItem(label, sc);
        ImGui::PopStyleColor();
        DrawShortcut(sc);
        return r;
    };

    constexpr float kMenuR = 5.0f;
    auto RMenuItem = [&](const char* label, const char* shortcut,
                         ImDrawFlags rf, bool* pChecked = nullptr) -> bool
    {
        if (rf == 0) return MItem(label, shortcut, pChecked);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->ChannelsSplit(2);
        dl->ChannelsSetCurrent(1);

        // Suppress ImGui's own square hover bg so only our rounded one shows
        ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  ImVec4(0,0,0,0));
        bool clicked = MItem(label, shortcut, pChecked);
        bool hov = ImGui::IsItemHovered();
        bool act = ImGui::IsItemActive();
        ImVec2 rMin = ImGui::GetItemRectMin();
        ImVec2 rMax = ImGui::GetItemRectMax();
        ImGui::PopStyleColor(3);

        if (hov || act) {
            dl->ChannelsSetCurrent(0);
            // Original (non-transparent) accent colors are restored after PopStyleColor
            ImU32 col = act ? ImGui::GetColorU32(ImGuiCol_HeaderActive)
                            : ImGui::GetColorU32(ImGuiCol_HeaderHovered);
            dl->AddRectFilled(rMin, rMax, col, kMenuR, rf);
        }
        dl->ChannelsMerge();
        return clicked;
    };

    auto PopupCloseCheck = [&]() {
        if (!ImGui::IsWindowAppearing()) {
            ImVec2 m = ImGui::GetIO().MousePos;
            ImVec2 p = ImGui::GetWindowPos(), s = ImGui::GetWindowSize();
            if (m.x < p.x || m.x > p.x + s.x || m.y < p.y - 32.0f || m.y > p.y + s.y)
                ImGui::CloseCurrentPopup();
        }
    };

    MenuBtn("File", "##mFile");
    ImGui::SetNextWindowPos(popupPos, ImGuiCond_Always);
    if (ImGui::BeginPopup("##mFile", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar)) {
        if (RMenuItem("New Project...",  nullptr, ImDrawFlags_RoundCornersTop)) {
            std::string pf = SaveProjectFileDialog();
            if (!pf.empty()) CreateProject(pf, std::filesystem::path(pf).stem().string());
        }
        if (MItem("Open Project...", nullptr)) {
            std::string pf = OpenProjectFileDialog();
            if (!pf.empty()) LoadProject(pf);
        }
        ImGui::Separator();
        if (MItem("New Scene", "Ctrl+N")) {
            NewDocument("Untitled");                       // adds a scene tab
            AddGameObject("Main Camera", PrimitiveType::Camera);
            m_selected = entt::null;
            LogSuccess("New scene created");
        }
        if (MItem("Open Scene", "Ctrl+O")) {
            std::string path = OpenSceneFileDialog();
            if (!path.empty()) OpenScene(path);            // opens in a new tab
        }
        ImGui::Separator();
        if (MItem("Save Scene",       "Ctrl+S"))       SaveCurrentScene();
        if (MItem("Save Scene As...", "Ctrl+Shift+S")) SaveDocument(m_activeDoc, true);
        ImGui::Separator();
        if (RMenuItem("Exit",    "Alt+F4", ImDrawFlags_RoundCornersBottom))
            RequestClose();
        PopupCloseCheck();
        ImGui::EndPopup();
    }

    MenuBtn("Edit", "##mEdit");
    ImGui::SetNextWindowPos(popupPos, ImGuiCond_Always);
    if (ImGui::BeginPopup("##mEdit", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar)) {
        if (RMenuItem("Undo", "Ctrl+Z", ImDrawFlags_RoundCornersTop))    Undo();
        if (MItem("Redo",     "Ctrl+Y"))                                  Redo();
        ImGui::Separator();
        MItem("Cut",  "Ctrl+X");
        MItem("Copy", "Ctrl+C");
        RMenuItem("Paste", "Ctrl+V", ImDrawFlags_RoundCornersBottom);
        PopupCloseCheck();
        ImGui::EndPopup();
    }

    MenuBtn("View", "##mView");
    ImGui::SetNextWindowPos(popupPos, ImGuiCond_Always);
    if (ImGui::BeginPopup("##mView", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar)) {
        RMenuItem("Scene Objects", nullptr, ImDrawFlags_RoundCornersTop,    &m_showHierarchy);
        ImGui::MenuItem("Inspector",        nullptr, &m_showInspector);
        ImGui::MenuItem("Asset Browser",    nullptr, &m_showAssetBrowser);
        RMenuItem("Console",       nullptr, ImDrawFlags_RoundCornersBottom, &m_showConsole);
        PopupCloseCheck();
        ImGui::EndPopup();
    }

    MenuBtn("GameObject", "##mGO");
    ImGui::SetNextWindowPos(popupPos, ImGuiCond_Always);
    if (ImGui::BeginPopup("##mGO", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar)) {
        if (RMenuItem("Create Empty", "Ctrl+Shift+N", ImDrawFlags_RoundCornersTop)) AddGameObject("GameObject", PrimitiveType::Empty);
        if (ImGui::MenuItem("Camera"))  AddGameObject("Camera",  PrimitiveType::Camera);
        if (ImGui::MenuItem("Light"))   AddGameObject("Light",   PrimitiveType::Light);
        ImGui::Separator();
        if (ImGui::MenuItem("Cube"))    AddGameObject("Cube",    PrimitiveType::Cube);
        if (ImGui::MenuItem("Sphere"))  AddGameObject("Sphere",  PrimitiveType::Sphere);
        if (ImGui::MenuItem("Plane"))   AddGameObject("Plane",   PrimitiveType::Plane);
        ImGui::Separator();
        if (RMenuItem("Import Model...", nullptr, ImDrawFlags_RoundCornersBottom)) {
            std::string src = OpenModelFileDialog();
            if (!src.empty())
                ImportModelAsync(src);
        }
        PopupCloseCheck();
        ImGui::EndPopup();
    }

    MenuBtn("Window", "##mWindow");
    ImGui::SetNextWindowPos(popupPos, ImGuiCond_Always);
    if (ImGui::BeginPopup("##mWindow", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar)) {
        ImGui::TextDisabled("Scenes");
        ImGui::Separator();
        for (SceneDoc& doc : m_docs) {
            bool active = (doc.id == m_activeDoc);
            std::string label = (doc.name.empty() ? "Untitled" : doc.name);
            if (doc.dirty) label += " *";
            label += "###win" + std::to_string(doc.id);
            if (ImGui::MenuItem(label.c_str(), nullptr, active) && !active)
                m_tabSelectId = doc.id;          // ask the viewport tab bar to switch
        }
        ImGui::Separator();
        if (ImGui::MenuItem("New Scene")) NewDocument("Untitled");
        PopupCloseCheck();
        ImGui::EndPopup();
    }

    MenuBtn("Help", "##mHelp", false);
    ImGui::SetNextWindowPos(popupPos, ImGuiCond_Always);
    if (ImGui::BeginPopup("##mHelp", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar)) {
        if (RMenuItem("About", nullptr, ImDrawFlags_RoundCornersAll))
            LogInfo(std::string(branding::kAppName) + " v" + branding::kVersion
                    + " - Built with C++ and ImGui");
        PopupCloseCheck();
        ImGui::EndPopup();
    }

    ImGui::PopStyleVar();

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(6);
}

// RenderMaterialEditor removed вЂ” use MaterialNodeEditor instead

void EditorUI::RenderAssetBrowser() {
    namespace fs = std::filesystem;

    if (!m_showAssetBrowser) return;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
    ImGui::Begin("Asset Browser", &m_showAssetBrowser, ImGuiWindowFlags_NoCollapse);
    ImGui::PopStyleVar();

    static double s_lastScan = -999.0;
    if (ImGui::GetTime() - s_lastScan > 2.0) {
        m_assetBrowserDirty = true;
        s_lastScan = ImGui::GetTime();
    }

    if (m_assetBrowserDirty) {
        m_assetBrowserDirty = false;
        m_assetBrowserItems.clear();
        static const std::unordered_set<std::string> kAssetExts = {
            ".emat", ".emdl", ".fcscn", ".escn", ".fcprefab",
            ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".hdr",
            ".obj", ".fbx", ".gltf", ".glb"
        };
        static const std::unordered_set<std::string> kHiddenDirs = {
            "Fonts", "fonts", "Icons", "icons"
        };

        if (fs::exists(m_assetBrowserPath)) {
            for (const auto& entry : fs::directory_iterator(m_assetBrowserPath)) {
                if (!entry.is_regular_file() && !entry.is_directory()) continue;

                AssetItem item;
                item.path  = entry.path().string();
                item.name  = entry.path().filename().string();
                item.isDir = entry.is_directory();
                item.ext   = entry.path().extension().string();
                for (auto& c : item.ext) c = (char)tolower((unsigned char)c);

                if (!item.name.empty() && item.name[0] == '.') continue;
                if (item.isDir && kHiddenDirs.count(item.name)) continue;
                if (!item.isDir && !kAssetExts.count(item.ext))  continue;

                m_assetBrowserItems.push_back(std::move(item));
            }
            std::sort(m_assetBrowserItems.begin(), m_assetBrowserItems.end(),
                [](const AssetItem& a, const AssetItem& b) {
                    if (a.isDir != b.isDir) return a.isDir > b.isDir;
                    return a.name < b.name;
                });

            // Compile node materials up front (once each) so thumbnails + scene
            // objects use the graph shader immediately, without opening the editor.
            if (m_nodeEditor) {
                for (const auto& it : m_assetBrowserItems) {
                    if (it.ext == ".emat" && !m_compiledGraphs.count(it.path)
                        && fs::exists(it.path + ".graph")) {
                        m_nodeEditor->EnsureCompiled(it.path, m_sceneRenderer, m_previewRenderer);
                        m_compiledGraphs.insert(it.path);
                    }
                }
            }
        }
    }

    fs::path cur(m_assetBrowserPath);
    bool canGoUp = cur.has_parent_path() && cur != fs::path("Assets") && cur != cur.root_path();

    if (!canGoUp) ImGui::BeginDisabled();
    if (ImGui::Button("<##up")) {
        m_assetBrowserPath     = cur.parent_path().string();
        m_assetBrowserSelected.clear();
        m_assetBrowserDirty    = true;
    }
    if (!canGoUp) ImGui::EndDisabled();

    ImGui::SameLine(0, 6);
    ImGui::TextDisabled("%s", m_assetBrowserPath.c_str());

    ImGui::Separator();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 2.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,  ImVec2(4.0f, 2.0f));
    if (ImGui::BeginTable("##abbar", 2, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("txt", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("btn", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%zu items", m_assetBrowserItems.size());
        if (!m_assetBrowserSelected.empty()) {
            ImGui::SameLine(0, 6);
            ImGui::TextDisabled("|  %s",
                fs::path(m_assetBrowserSelected).filename().string().c_str());
        }
        ImGui::TableSetColumnIndex(1);
        if (ImGui::Button("New Material", ImVec2(-1, 0))) {
            fs::create_directories("Assets/Materials");
            int idx = 1;
            std::string path;
            do {
                path = "Assets/Materials/Material_" + std::to_string(idx++) + ".emat";
            } while (fs::exists(path));
            Material mat;
            if (mat.Save(path)) {
                // Remove stale graph file from previous deleted material with same name
                std::error_code ec;
                fs::remove(path + ".graph", ec);
                MaterialManager::Invalidate(path);
                if (m_previewRenderer) m_previewRenderer->InvalidatePreview(path);
                m_assetBrowserDirty = true;
                if (m_nodeEditor) m_nodeEditor->Open(path, m_previewRenderer, m_sceneRenderer);
                LogSuccess("Created: " + fs::path(path).filename().string());
            }
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar(2);

    ImGui::Separator();

    constexpr float kCell = 72.0f;
    constexpr float kPad  =  6.0f;
    constexpr float kNameH = 18.0f;
    const float cellTotal = kCell + kPad * 2.0f;

    float panelW  = ImGui::GetContentRegionAvail().x;
    int   columns = std::max(1, (int)(panelW / cellTotal));

    ImGui::BeginChild("##assetGrid", ImVec2(0.0f, 0.0f), false);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

    for (int i = 0; i < (int)m_assetBrowserItems.size(); ++i) {
        const AssetItem& item = m_assetBrowserItems[i];

        if (i % columns != 0)
            ImGui::SameLine(0.0f, kPad * 2.0f);

        ImVec4 typeColor;
        const char* typeLabel;
        if (item.isDir) {
            typeColor = {0.82f, 0.60f, 0.20f, 1.0f}; typeLabel = "DIR";
        } else if (item.ext == ".emdl") {
            typeColor = {0.20f, 0.72f, 0.82f, 1.0f}; typeLabel = "MDL";
        } else if (item.ext==".obj"||item.ext==".fbx"||item.ext==".gltf"||item.ext==".glb") {
            typeColor = {0.90f, 0.52f, 0.12f, 1.0f}; typeLabel = "SRC";
        } else if (item.ext == ".fcscn" || item.ext == ".escn") {
            typeColor = {0.62f, 0.32f, 0.92f, 1.0f}; typeLabel = "SCN";
        } else if (item.ext == ".fcprefab") {
            typeColor = {0.30f, 0.74f, 0.52f, 1.0f}; typeLabel = "PRE";
        } else if (item.ext==".png"||item.ext==".jpg"||item.ext==".jpeg"||item.ext==".bmp") {
            typeColor = {0.90f, 0.80f, 0.12f, 1.0f}; typeLabel = "IMG";
        } else {
            typeColor = {0.48f, 0.48f, 0.52f, 1.0f}; typeLabel = "?";
        }

        bool selected = (item.path == m_assetBrowserSelected);

        ImGui::PushID(i);
        bool clicked = ImGui::InvisibleButton("##cell",
                           ImVec2(cellTotal, kCell + kPad * 2.0f + kNameH));
        bool hovered  = ImGui::IsItemHovered();
        bool dblClick = hovered && ImGui::IsMouseDoubleClicked(0);
        ImVec2 rMin = ImGui::GetItemRectMin();
        ImVec2 rMax = ImGui::GetItemRectMax();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Context menu вЂ” BeginPopupContextItem must be directly after the item
        ImGui::SetNextWindowSizeConstraints(ImVec2(150.0f, 0), ImVec2(300.0f, FLT_MAX));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(3.0f, 3.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(0.0f, 0.0f));
        if (ImGui::BeginPopupContextItem("##ctx", ImGuiPopupFlags_MouseButtonRight)) {
            constexpr float kCtxR    = 5.0f;
            constexpr float kItemH   = 24.0f;
            constexpr float kTextPad = 10.0f;
            const     float kW       = ImGui::GetContentRegionAvail().x;
            auto CItem = [&](const char* label, ImDrawFlags rf) -> bool {
                ImVec2    pos = ImGui::GetCursorScreenPos();
                bool      r   = ImGui::InvisibleButton(label, ImVec2(kW, kItemH));
                bool      hv  = ImGui::IsItemHovered();
                bool      ac  = ImGui::IsItemActive();
                ImDrawList* cdl = ImGui::GetWindowDrawList();
                if (hv || ac) {
                    ImU32 col = ac ? ImGui::GetColorU32(ImGuiCol_HeaderActive)
                                   : ImGui::GetColorU32(ImGuiCol_HeaderHovered);
                    cdl->AddRectFilled(pos, {pos.x + kW, pos.y + kItemH}, col, kCtxR, rf);
                }
                float textY = pos.y + (kItemH - ImGui::GetTextLineHeight()) * 0.5f;
                cdl->AddText({pos.x + kTextPad, textY},
                             ImGui::GetColorU32(ImGuiCol_Text), label);
                return r;
            };
            if (item.ext == ".emat") {
                if (CItem("Open Editor",      ImDrawFlags_RoundCornersTop))    { if (m_nodeEditor) m_nodeEditor->Open(item.path, m_previewRenderer, m_sceneRenderer); ImGui::CloseCurrentPopup(); }
                if (CItem("Show in Explorer", ImDrawFlags_None))               { std::string a="/select,\""+item.path+"\""; ShellExecuteA(nullptr,"open","explorer.exe",a.c_str(),nullptr,SW_SHOWNORMAL); ImGui::CloseCurrentPopup(); }
                ImGui::Separator();
                if (CItem("Delete",           ImDrawFlags_RoundCornersBottom)) {
                    std::error_code ec;
                    if (m_sceneRenderer)   m_sceneRenderer->UnregisterNodeShader(item.path);
                    if (m_previewRenderer) m_previewRenderer->InvalidatePreview(item.path);
                    MaterialManager::Invalidate(item.path);
                    m_compiledGraphs.erase(item.path);
                    fs::remove(item.path,          ec);
                    fs::remove(item.path + ".meta",  ec);
                    fs::remove(item.path + ".graph", ec);
                    m_assetBrowserDirty = true;
                    m_assetBrowserSelected.clear();
                    ImGui::CloseCurrentPopup();
                }
            } else {
                if (CItem("Show in Explorer", ImDrawFlags_RoundCornersTop))    { std::string a="/select,\""+item.path+"\""; ShellExecuteA(nullptr,"open","explorer.exe",a.c_str(),nullptr,SW_SHOWNORMAL); ImGui::CloseCurrentPopup(); }
                if (CItem("Delete",           ImDrawFlags_RoundCornersBottom)) {
                    std::error_code ec;
                    fs::remove(item.path,         ec);
                    fs::remove(item.path + ".meta", ec);
                    m_assetBrowserDirty = true;
                    m_assetBrowserSelected.clear();
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);

        if (selected)
            dl->AddRectFilled(rMin, rMax, IM_COL32(192, 54, 76, 45), 5.0f);
        else if (hovered)
            dl->AddRectFilled(rMin, rMax, IM_COL32(255, 255, 255, 12), 5.0f);
        if (selected)
            dl->AddRect(rMin, rMax, IM_COL32(192, 54, 76, 180), 5.0f, 0, 1.5f);

        ImVec2 icMin = {rMin.x + kPad, rMin.y + kPad};
        ImVec2 icMax = {icMin.x + kCell, icMin.y + kCell};

        bool drewPreview = false;
        if (item.ext == ".emat" && m_previewRenderer) {
            Material* emat = MaterialManager::GetOrLoad(item.path);
            if (emat) {
                uint32_t prev = m_previewRenderer->GetPreview(emat, item.path);
                if (prev) {
                    dl->AddImageRounded((ImTextureID)(intptr_t)prev,
                        icMin, icMax, ImVec2(0,1), ImVec2(1,0),
                        IM_COL32_WHITE, 6.0f);
                    dl->AddRect(icMin, icMax, IM_COL32(255,255,255,30), 6.0f, 0, 1.0f);
                    drewPreview = true;
                }
            }
        }
        if (!drewPreview) {
            ImU32 icBg  = IM_COL32((int)(typeColor.x*60),(int)(typeColor.y*60),(int)(typeColor.z*60),255);
            ImU32 icBrd = IM_COL32((int)(typeColor.x*220),(int)(typeColor.y*220),(int)(typeColor.z*220),120);
            dl->AddRectFilled(icMin, icMax, icBg, 6.0f);
            dl->AddRect(icMin, icMax, icBrd, 6.0f, 0, 1.2f);
            ImU32  icText = ImGui::ColorConvertFloat4ToU32(typeColor);
            ImVec2 lSz    = ImGui::CalcTextSize(typeLabel);
            dl->AddText({icMin.x + (kCell - lSz.x) * 0.5f, icMin.y + (kCell - lSz.y) * 0.5f},
                        icText, typeLabel);
        }

        std::string dispName = item.name;
        while (!dispName.empty()) {
            ImVec2 sz = ImGui::CalcTextSize(dispName.c_str());
            if (sz.x <= cellTotal - 4.0f) break;
            dispName.pop_back();
        }
        if (dispName.size() < item.name.size() && dispName.size() >= 2) {
            dispName.back() = '.'; dispName += '.';
        }
        ImVec2 nSz = ImGui::CalcTextSize(dispName.c_str());
        dl->AddText({rMin.x + (cellTotal - nSz.x) * 0.5f, icMax.y + 4.0f},
                    IM_COL32(195, 195, 205, 255), dispName.c_str());

        if (clicked) m_assetBrowserSelected = item.path;

        if (dblClick) {
            if (item.isDir) {
                m_assetBrowserPath     = item.path;
                m_assetBrowserSelected.clear();
                m_assetBrowserDirty    = true;
            } else if (item.ext == ".emat") {
                if (m_nodeEditor)
                    m_nodeEditor->Open(item.path, m_previewRenderer, m_sceneRenderer);
            } else if (item.ext == ".emdl") {
                AddModelObject(item.path);
            } else if (item.ext==".obj"||item.ext==".fbx"||item.ext==".gltf"||item.ext==".glb") {
                ImportModelAsync(item.path);
            } else if (item.ext == ".fcscn" || item.ext == ".escn") {
                OpenScene(item.path);
            } else if (item.ext == ".fcprefab") {
                InstantiatePrefab(item.path);
            } else if (item.ext == ".hdr") {
                if (m_sceneRenderer) {
                    m_sceneRenderer->SetEnvironmentHDR(item.path);
                    LogSuccess("Environment set: " + item.name);
                }
            }
        }

        if (item.ext == ".emat" && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("ASSET_EMAT", item.path.c_str(), item.path.size() + 1);
            namespace fs = std::filesystem;
            bool shownPrev = false;
            if (m_previewRenderer) {
                Material* dragMat = MaterialManager::GetOrLoad(item.path);
                if (dragMat) {
                    uint32_t prev = m_previewRenderer->GetPreview(dragMat, item.path);
                    if (prev) {
                        ImGui::Image((ImTextureID)(intptr_t)prev,
                            ImVec2(48, 48), ImVec2(0,1), ImVec2(1,0));
                        ImGui::SameLine(0, 6);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 16.0f);
                        shownPrev = true;
                    }
                }
            }
            if (!shownPrev) ImGui::TextDisabled(ICON_FA_CIRCLE "  ");
            ImGui::Text("%s", fs::path(item.path).stem().string().c_str());
            ImGui::EndDragDropSource();
        }

        if (hovered && !ImGui::IsMouseDown(ImGuiMouseButton_Right) && ImGui::BeginTooltip()) {
            ImGui::Text("%s", item.name.c_str());
            ImGui::TextDisabled("%s", item.path.c_str());
            ImGui::EndTooltip();
        }


        ImGui::PopID();
    }

    if (m_assetBrowserItems.empty()) {
        ImGui::TextDisabled("(empty)");
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();

    ImGui::End();
}

void EditorUI::RenderHierarchy() {
    if (!m_showHierarchy) return;

    ImGui::Begin("Scene Objects", &m_showHierarchy, ImGuiWindowFlags_NoCollapse);

    auto& reg = m_scene.Reg();
    auto  nameView = reg.view<NameComponent>();
    ImGui::TextDisabled("%s  \xc2\xb7  %zu objects",
                        ActiveDoc().name.c_str(), (size_t)nameView.size());
    ImGui::Separator();

    // Render the forest: only root entities at the top level; RenderEntityNode
    // recurses into children. Snapshot root ids first so a Delete/Duplicate/
    // reparent during iteration can't invalidate the view.
    std::vector<entt::entity> roots;
    for (entt::entity e : nameView) {
        const auto* pc = reg.try_get<ParentComponent>(e);
        if (!pc || pc->parent == entt::null || !reg.valid(pc->parent)) roots.push_back(e);
    }
    for (entt::entity e : roots)
        if (m_scene.Valid(e)) RenderEntityNode(e);

    ImGui::Separator();

    if (ImGui::Button("New Scene")) NewDocument("Untitled");
    ImGui::SameLine();
    if (ImGui::Button("Add Object")) {
        std::string sourcePath = OpenModelFileDialog();
        if (!sourcePath.empty())
            ImportModelAsync(sourcePath);
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete Selected")) {
        if (m_scene.Valid(m_selected)) {
            std::string name = reg.get<NameComponent>(m_selected).name;
            DestroyEntitySubtree(m_selected);   // clears m_selected
            MarkDirty();
            LogInfo("Deleted: " + name);
        }
    }

    ImGui::End();
}

void EditorUI::RenderEntityNode(entt::entity e) {
    entt::registry& reg = m_scene.Reg();
    std::string name = reg.get<NameComponent>(e).name;   // copy: survives a Delete below
    const MaterialComponent* matc = reg.try_get<MaterialComponent>(e);
    std::string matPath = matc ? matc->path : std::string();

    // Children = entities whose parent is e. Snapshot now so a reparent/delete
    // triggered by this node's drop/menu can't invalidate the recursion below.
    std::vector<entt::entity> children;
    for (entt::entity c : reg.view<ParentComponent>())
        if (reg.get<ParentComponent>(c).parent == e) children.push_back(c);

    ImGui::PushID(static_cast<int>(entt::to_integral(e)));

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                               ImGuiTreeNodeFlags_OpenOnDoubleClick |
                               ImGuiTreeNodeFlags_SpanAvailWidth |
                               ImGuiTreeNodeFlags_DefaultOpen;
    if (m_selected == e)  flags |= ImGuiTreeNodeFlags_Selected;
    if (children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;

    bool open = ImGui::TreeNodeEx((void*)(intptr_t)entt::to_integral(e),
                                  flags, "%s", name.c_str());

    // Single-click the label selects; clicking the arrow only toggles open.
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        SelectEntity(e);
        LogInfo("Selected: " + name);
    }

    // Drag SOURCE: pick this entity up to drop onto a new parent (or empty space).
    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("ENTITY_NODE", &e, sizeof(entt::entity));
        ImGui::Text("%s", name.c_str());
        ImGui::EndDragDropSource();
    }

    // Drag TARGET: another entity (reparent onto this), or a material (assign).
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ENTITY_NODE")) {
            entt::entity dragged = *static_cast<const entt::entity*>(p->Data);
            ReparentEntity(dragged, e);
        }
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ASSET_EMAT")) {
            std::string assigned(static_cast<const char*>(p->Data));
            reg.emplace_or_replace<MaterialComponent>(e, MaterialComponent{ assigned });
            if (m_nodeEditor) m_nodeEditor->EnsureCompiled(assigned, m_sceneRenderer);
            MarkDirty();
            LogInfo("Material assigned to " + name);
        }
        ImGui::EndDragDropTarget();
    }

    if (!matPath.empty() && m_previewRenderer) {
        Material* hmat = MaterialManager::GetOrLoad(matPath);
        if (hmat) {
            uint32_t prev = m_previewRenderer->GetPreview(hmat, matPath);
            if (prev) {
                constexpr float kThumbSz = 16.0f;
                ImVec2 iMax = ImGui::GetItemRectMax();
                ImVec2 iMin = ImGui::GetItemRectMin();
                float  cy   = iMin.y + (iMax.y - iMin.y - kThumbSz) * 0.5f;
                ImVec2 tMin = {iMax.x - kThumbSz - 3.0f, cy};
                ImVec2 tMax = {tMin.x + kThumbSz, tMin.y + kThumbSz};
                ImGui::GetWindowDrawList()->AddImageRounded(
                    (ImTextureID)(intptr_t)prev,
                    tMin, tMax, ImVec2(0,1), ImVec2(1,0),
                    IM_COL32_WHITE, 3.0f);
            }
        }
    }

    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Duplicate")) {
            DuplicateEntity(e);
            MarkDirty();
            LogInfo("Duplicated: " + name);
        }
        if (ImGui::MenuItem("Create Prefab...")) CreatePrefab(e);
        const auto* pc = reg.try_get<ParentComponent>(e);
        bool hasParent = pc && pc->parent != entt::null && reg.valid(pc->parent);
        if (ImGui::MenuItem("Unparent", nullptr, false, hasParent))
            ReparentEntity(e, entt::null);
        ImGui::Separator();
        if (ImGui::MenuItem("Delete")) {
            DestroyEntitySubtree(e);
            MarkDirty();
            LogInfo("Deleted: " + name);
        }
        ImGui::EndPopup();
    }

    if (open) {
        for (entt::entity c : children)
            if (m_scene.Valid(c)) RenderEntityNode(c);
        ImGui::TreePop();
    }

    ImGui::PopID();
}

// Reparent `child` under `newParent` (null = make a root), preserving the child's
// world pose so it doesn't visually jump. Rejects cycles (parenting to self or a
// descendant).
void EditorUI::ReparentEntity(entt::entity child, entt::entity newParent) {
    if (child == entt::null || !m_scene.Valid(child) || child == newParent) return;
    entt::registry& reg = m_scene.Reg();
    if (newParent != entt::null) {
        if (!reg.valid(newParent)) return;
        if (IsAncestorOf(reg, child, newParent)) return;   // would create a cycle
    }

    // Keep the world transform constant: newLocal = inverse(parentWorld) * childWorld.
    glm::mat4 childWorld = WorldMatrixOf(reg, child);
    glm::mat4 parentWorld(1.0f);
    if (newParent != entt::null) parentWorld = WorldMatrixOf(reg, newParent);
    SetTransformFromMatrix(reg.get<TransformComponent>(child),
                           glm::inverse(parentWorld) * childWorld);

    if (newParent == entt::null) reg.remove<ParentComponent>(child);
    else reg.emplace_or_replace<ParentComponent>(child, ParentComponent{ newParent });
    MarkDirty();
}

// Destroy an entity together with its whole subtree (children, grandchildren, ...).
void EditorUI::DestroyEntitySubtree(entt::entity e) {
    if (!m_scene.Valid(e)) return;
    entt::registry& reg = m_scene.Reg();
    std::vector<entt::entity> kids;
    for (entt::entity c : reg.view<ParentComponent>())
        if (reg.get<ParentComponent>(c).parent == e) kids.push_back(c);
    for (entt::entity c : kids) DestroyEntitySubtree(c);
    if (m_selected == e) m_selected = entt::null;
    m_scene.Destroy(e);
}

entt::entity EditorUI::DuplicateEntity(entt::entity src) {
    entt::registry& reg = m_scene.Reg();
    entt::entity e = m_scene.Create(reg.get<NameComponent>(src).name + "_copy");
    CopyEntity(reg, src, reg, e);
    // The copy shares the original's parent (a sibling); CopyEntity skips the link.
    if (const auto* pc = reg.try_get<ParentComponent>(src);
        pc && pc->parent != entt::null && reg.valid(pc->parent))
        reg.emplace<ParentComponent>(e, *pc);
    return e;
}

// A prefab is just a one-entity scene, so it reuses the scene serializer.
void EditorUI::CreatePrefab(entt::entity e) {
    if (!m_scene.Valid(e)) return;
    std::string defName = m_scene.Reg().get<NameComponent>(e).name;
    std::string path = SavePrefabFileDialog(m_project.assetDir, defName);
    if (path.empty()) return;

    Scene temp;
    entt::entity n = temp.Create(defName);
    CopyEntity(m_scene.Reg(), e, temp.Reg(), n);
    if (!SceneSerializer::Save(path, temp)) {
        LogError("Failed to save prefab: " + path);
        return;
    }
    AssetDatabase::GetOrCreateGuid(path, "Prefab");
    m_assetBrowserDirty = true;
    LogSuccess("Prefab created: " + std::filesystem::path(path).filename().string());
}

void EditorUI::InstantiatePrefab(const std::string& path) {
    Scene temp;
    if (!SceneSerializer::Load(path, temp)) {
        LogError("Failed to load prefab: " + path);
        return;
    }
    entt::registry&       reg  = m_scene.Reg();
    const entt::registry& treg = temp.Reg();
    std::unordered_map<entt::entity, entt::entity> map;
    entt::entity last = entt::null;
    for (entt::entity e : treg.view<NameComponent>()) {
        entt::entity n = m_scene.Create(treg.get<NameComponent>(e).name);
        CopyEntity(treg, e, reg, n);
        map[e] = n;
        last = n;
    }
    // Re-link any internal parent relationships; prefab roots stay at scene root.
    for (auto [se, de] : map) {
        if (const auto* pc = treg.try_get<ParentComponent>(se)) {
            auto it = map.find(pc->parent);
            if (it != map.end()) reg.emplace<ParentComponent>(de, ParentComponent{ it->second });
        }
    }
    if (last != entt::null) {
        m_selected = last;
        MarkDirty();
        LogSuccess("Instantiated prefab: " + std::filesystem::path(path).stem().string());
    }
}

void EditorUI::EnterPlayMode() {
    if (m_playState != PlayState::Editing) return;

    // Snapshot the whole scene into a parallel registry so Stop restores cleanly.
    m_playBackup.Clear();
    m_playMap.clear();
    entt::registry& reg  = m_scene.Reg();
    entt::registry& breg = m_playBackup.Reg();
    for (entt::entity e : reg.view<NameComponent>()) {
        entt::entity b = m_playBackup.Create(reg.get<NameComponent>(e).name);
        CopyEntity(reg, e, breg, b);
        m_playMap[e] = b;
    }

    m_playTime  = 0.0f;
    m_playState = PlayState::Playing;
    if (m_physics) m_physics->Begin(m_scene);   // build rigid bodies from components
    if (m_physics && m_physics->HasCharacter()) {   // capture the mouse for FPS control
        m_playYaw = 0.0f; m_playPitch = 0.0f;
        m_mouseCaptured = true;
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
    if (m_lua) m_lua->StartAll(m_scene);   // run script OnStart
    LogSuccess("Play mode: started");
}

void EditorUI::ExitPlayMode() {
    if (m_playState == PlayState::Editing) return;
    if (m_lua) m_lua->StopAll();              // run script OnDestroy, clear state
    if (m_physics) m_physics->End();          // destroy rigid bodies
    if (m_mouseCaptured) {                     // release the captured cursor
        m_mouseCaptured = false;
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    // Entities are never created/destroyed during play, so ids stay valid and
    // selection is preserved. Only transforms change, so that's all we restore.
    entt::registry&       reg  = m_scene.Reg();
    const entt::registry& breg = m_playBackup.Reg();
    for (auto [live, backup] : m_playMap) {
        if (reg.valid(live) && breg.valid(backup))
            reg.get<TransformComponent>(live) = breg.get<TransformComponent>(backup);
    }
    m_playBackup.Clear();
    m_playMap.clear();
    m_playState = PlayState::Editing;
    LogInfo("Play mode: stopped (scene restored)");
}

void EditorUI::TogglePause() {
    if (m_playState == PlayState::Playing)      m_playState = PlayState::Paused;
    else if (m_playState == PlayState::Paused)  m_playState = PlayState::Playing;
}

void EditorUI::UpdatePlayMode() {
    if (m_playState != PlayState::Playing) return;
    float dt = ImGui::GetIO().DeltaTime;
    m_playTime += dt;

    entt::registry&       reg  = m_scene.Reg();
    const entt::registry& breg = m_playBackup.Reg();

    // Built-in behaviors are evaluated relative to the play-start snapshot, so
    // they're deterministic and Stop cleanly restores the scene.
    auto baseOf = [&](entt::entity e) -> const TransformComponent* {
        auto it = m_playMap.find(e);
        if (it == m_playMap.end() || !breg.valid(it->second)) return nullptr;
        return &breg.get<TransformComponent>(it->second);
    };

    for (auto [e, rot] : reg.view<RotatorComponent>().each()) {
        if (!rot.enabled) continue;
        const TransformComponent* base = baseOf(e);
        if (!base) continue;
        glm::vec3 ax = rot.axis;
        if (glm::length(ax) < 1e-4f) ax = glm::vec3(0, 1, 0);
        ax = glm::normalize(ax);
        reg.get<TransformComponent>(e).rotation =
            glm::angleAxis(glm::radians(rot.speed * m_playTime), ax) * base->rotation;
    }

    for (auto [e, flo] : reg.view<FloaterComponent>().each()) {
        if (!flo.enabled) continue;
        const TransformComponent* base = baseOf(e);
        if (!base) continue;
        glm::vec3 ax = flo.axis;
        if (glm::length(ax) < 1e-4f) ax = glm::vec3(0, 1, 0);
        ax = glm::normalize(ax);
        float off = std::sin(m_playTime * flo.speed) * flo.amount;
        reg.get<TransformComponent>(e).position = base->position + ax * off;
    }

    if (m_lua) m_lua->UpdateAll(m_scene, dt);   // script OnUpdate(dt)
    if (m_physics) m_physics->Step(m_scene, dt); // advance bodies, write transforms back
}

// Play-mode character input: mouse-look (while captured) + WASD + jump, fed to
// the physics character. Called from RenderViewport before the follow camera is
// built so the look direction is fresh.
void EditorUI::UpdateCharacterInput(const CharacterControllerComponent& cc) {
    ImGuiIO& io = ImGui::GetIO();

    if (m_mouseCaptured) {
        m_playYaw   += io.MouseDelta.x * cc.mouseSensitivity;
        m_playPitch -= io.MouseDelta.y * cc.mouseSensitivity;
        m_playPitch  = std::clamp(m_playPitch, -1.55f, 1.55f);   // ~±89°
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            m_mouseCaptured = false;
            glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }

    // Horizontal basis from yaw (forward = -Z at yaw 0).
    glm::vec3 fwd(  std::sin(m_playYaw), 0.0f, -std::cos(m_playYaw));
    glm::vec3 right(std::cos(m_playYaw), 0.0f,  std::sin(m_playYaw));
    glm::vec3 move(0.0f);
    if (ImGui::IsKeyDown(ImGuiKey_W)) move += fwd;
    if (ImGui::IsKeyDown(ImGuiKey_S)) move -= fwd;
    if (ImGui::IsKeyDown(ImGuiKey_D)) move += right;
    if (ImGui::IsKeyDown(ImGuiKey_A)) move -= right;
    if (glm::length(move) > 1e-4f) move = glm::normalize(move);

    bool jump = ImGui::IsKeyPressed(ImGuiKey_Space, false);
    if (m_physics) m_physics->SetCharacterInput(move, jump);
}

void EditorUI::RenderViewportToolbar() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(30, 30, 35, 255));
    ImGui::BeginChild("##vptoolbar", ImVec2(0, kViewportToolbarH), false,
                      ImGuiWindowFlags_NoScrollbar);

    const ImVec2 bsz(30.0f, 26.0f);
    ImGui::SetCursorPos(ImVec2(8.0f, (kViewportToolbarH - bsz.y) * 0.5f));

    // Play / Pause toggle.
    const bool playing = (m_playState == PlayState::Playing);
    const bool editing = (m_playState == PlayState::Editing);
    const char* playIcon = playing ? ICON_FA_PAUSE : ICON_FA_PLAY;
    ImU32 playCol = editing ? IM_COL32(120, 210, 120, 255)   // green "play"
                            : IM_COL32(235, 200, 90, 255);    // amber "pause/resume"
    if (IconButton(playIcon, "##vp_play", bsz, playCol)) {
        if (editing) EnterPlayMode();
        else         TogglePause();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(editing ? "Play" : (playing ? "Pause" : "Resume"));

    ImGui::SameLine(0, 4);
    ImU32 stopCol = editing ? IM_COL32(110, 110, 118, 255)    // dimmed when nothing to stop
                            : IM_COL32(220, 90, 90, 255);     // red "stop"
    if (IconButton(ICON_FA_STOP, "##vp_stop", bsz, stopCol)) {
        if (!editing) ExitPlayMode();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stop");

    // Render screenshot.
    ImGui::SameLine(0, 14);
    if (IconButton(ICON_FA_IMAGE, "##vp_shot", bsz)) {
        namespace fs = std::filesystem;
        std::error_code ec;
        fs::create_directories("Assets/Screenshots", ec);
        int n = 1; std::string p;
        do { p = "Assets/Screenshots/Screenshot_" + std::to_string(n++) + ".bmp"; }
        while (fs::exists(p));
        if (m_sceneRenderer && m_sceneRenderer->SaveScreenshot(p))
            LogSuccess("Screenshot saved: " + p);
        else
            LogError("Screenshot failed");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Render screenshot");

    // Render settings (post-processing) — gear pinned to the right.
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - bsz.x - 8.0f);
    if (IconButton(ICON_FA_GEAR, "##vp_settings", bsz)) ImGui::OpenPopup("##postfx");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Render settings");

    ImGui::SetNextWindowSize(ImVec2(270, 0));
    if (ImGui::BeginPopup("##postfx")) {
        ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.88f, 1.0f), "Post-Processing");
        ImGui::Separator();
        ImGui::Spacing();
        if (m_sceneRenderer) {
            float exposure = m_sceneRenderer->GetExposure();
            ImGui::TextDisabled("Exposure");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat("##exposure", &exposure, 0.01f, 0.05f, 8.0f, "%.2f"))
                m_sceneRenderer->SetExposure(exposure);

            ImGui::Spacing();
            ImGui::TextDisabled("Anti-Aliasing");
            const char* aaNames[] = { "None", "FXAA" };
            int aa = (int)m_sceneRenderer->GetAAMode();
            ImGui::SetNextItemWidth(-1);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
            if (ImGui::BeginCombo("##aamode", aaNames[aa])) {
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(192, 54, 76, 160));
                ImGui::PushStyleColor(ImGuiCol_Header,        IM_COL32(192, 54, 76, 200));
                for (int i = 0; i < IM_ARRAYSIZE(aaNames); ++i) {
                    bool sel = (aa == i);
                    if (ImGui::Selectable(aaNames[i], sel))
                        m_sceneRenderer->SetAAMode((SceneRenderer::AAMode)i);
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::PopStyleColor(2);
                ImGui::EndCombo();
            }
            ImGui::PopStyleVar();

            ImGui::Spacing();
            bool bloom = m_sceneRenderer->GetBloomEnabled();
            if (ImGui::Checkbox("Bloom", &bloom))
                m_sceneRenderer->SetBloomEnabled(bloom);
            if (bloom) {
                float th = m_sceneRenderer->GetBloomThreshold();
                ImGui::TextDisabled("Threshold");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##bloomth", &th, 0.01f, 0.0f, 5.0f, "%.2f"))
                    m_sceneRenderer->SetBloomThreshold(th);
                float inten = m_sceneRenderer->GetBloomIntensity();
                ImGui::TextDisabled("Intensity");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##bloomint", &inten, 0.01f, 0.0f, 3.0f, "%.2f"))
                    m_sceneRenderer->SetBloomIntensity(inten);
            }

            ImGui::Spacing();
            bool ssao = m_sceneRenderer->GetSSAOEnabled();
            if (ImGui::Checkbox("SSAO", &ssao))
                m_sceneRenderer->SetSSAOEnabled(ssao);
            if (ssao) {
                float rad = m_sceneRenderer->GetSSAORadius();
                ImGui::TextDisabled("Radius");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##ssaorad", &rad, 0.01f, 0.05f, 3.0f, "%.2f"))
                    m_sceneRenderer->SetSSAORadius(rad);
                float pw = m_sceneRenderer->GetSSAOPower();
                ImGui::TextDisabled("Strength");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##ssaopow", &pw, 0.02f, 0.1f, 6.0f, "%.2f"))
                    m_sceneRenderer->SetSSAOPower(pw);
            }

            ImGui::Spacing();
            bool ssr = m_sceneRenderer->GetSSREnabled();
            if (ImGui::Checkbox("SSR (reflections)", &ssr))
                m_sceneRenderer->SetSSREnabled(ssr);
            if (ssr) {
                float si = m_sceneRenderer->GetSSRIntensity();
                ImGui::TextDisabled("Intensity");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##ssrint", &si, 0.01f, 0.0f, 2.0f, "%.2f"))
                    m_sceneRenderer->SetSSRIntensity(si);
            }

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.88f, 1.0f), "Color Grading");
            ImGui::Separator();
            ImGui::Spacing();

            float contrast = m_sceneRenderer->GetContrast();
            ImGui::TextDisabled("Contrast");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat("##contrast", &contrast, 0.005f, 0.5f, 2.0f, "%.2f"))
                m_sceneRenderer->SetContrast(contrast);

            float sat = m_sceneRenderer->GetSaturation();
            ImGui::TextDisabled("Saturation");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat("##saturation", &sat, 0.005f, 0.0f, 2.0f, "%.2f"))
                m_sceneRenderer->SetSaturation(sat);

            float temp = m_sceneRenderer->GetTemperature();
            ImGui::TextDisabled("Temperature");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat("##temperature", &temp, 0.005f, -1.0f, 1.0f, "%.2f"))
                m_sceneRenderer->SetTemperature(temp);

            float vig = m_sceneRenderer->GetVignette();
            ImGui::TextDisabled("Vignette");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat("##vignette", &vig, 0.005f, 0.0f, 1.0f, "%.2f"))
                m_sceneRenderer->SetVignette(vig);
        }
        ImGui::EndPopup();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void EditorUI::RenderViewportTimeline() {
    // Pin to the bottom of the viewport window.
    ImGui::SetCursorPos(ImVec2(0.0f, ImGui::GetWindowHeight() - kViewportTimelineH));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(24, 24, 28, 255));
    ImGui::BeginChild("##vptimeline", ImVec2(0, kViewportTimelineH), true,
                      ImGuiWindowFlags_NoScrollbar);
    ImGui::TextDisabled("Timeline");
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void EditorUI::RenderSceneTabs() {
    ImGuiTabBarFlags barFlags = ImGuiTabBarFlags_Reorderable
                              | ImGuiTabBarFlags_AutoSelectNewTabs
                              | ImGuiTabBarFlags_FittingPolicyScroll;
    if (!ImGui::BeginTabBar("##scenetabs", barFlags)) return;

    uint32_t selected = 0;   // doc id whose tab ImGui has selected this frame
    uint32_t closeReq  = 0;
    for (SceneDoc& doc : m_docs) {
        std::string label = doc.name.empty() ? "Untitled" : doc.name;
        if (doc.dirty) label += " *";
        label += "###tab" + std::to_string(doc.id);   // stable id; visible text may change

        ImGuiTabItemFlags itemFlags = ImGuiTabItemFlags_None;
        if (m_tabSelectId == doc.id) itemFlags |= ImGuiTabItemFlags_SetSelected;

        bool open = true;
        if (ImGui::BeginTabItem(label.c_str(), &open, itemFlags)) {
            selected = doc.id;
            ImGui::EndTabItem();
        }
        if (!open) closeReq = doc.id;
    }
    m_tabSelectId = 0;   // consume the programmatic-select request

    ImGui::EndTabBar();

    if (selected && selected != m_activeDoc) SwitchTo(selected);   // user clicked a tab
    if (closeReq) RequestCloseDocument(closeReq);                  // prompt if unsaved
}

void EditorUI::RenderViewport() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    // Browser-style scene tabs at the very top.
    RenderSceneTabs();

    // Icon toolbar above the 3D view (play / render / settings).
    RenderViewportToolbar();

    // The 3D view fills the space between the toolbar and the bottom timeline.
    ImVec2 viewportSize = ImGui::GetContentRegionAvail();
    viewportSize.y = (viewportSize.y > kViewportTimelineH + 1.0f)
                     ? viewportSize.y - kViewportTimelineH : 1.0f;
    m_viewportWidth  = viewportSize.x;
    m_viewportHeight = viewportSize.y;

    const bool editing = (m_playState == PlayState::Editing);

    float aspect = (viewportSize.y > 0) ? viewportSize.x / viewportSize.y : 1.0f;
    glm::mat4 view       = m_camera.GetViewMatrix();
    glm::mat4 projection = m_camera.GetProjectionMatrix(aspect);

    // In play mode the camera follows the character controller if there is one;
    // otherwise it renders through the scene's first camera entity (game view).
    bool charCamActive = false;
    if (!editing) {
        entt::registry& reg = m_scene.Reg();

        entt::entity ce = entt::null;
        const CharacterControllerComponent* cc = nullptr;
        for (auto [e, c] : reg.view<CharacterControllerComponent>().each()) { ce = e; cc = &c; break; }

        if (ce != entt::null && cc && m_physics && m_physics->HasCharacter()) {
            charCamActive = true;
            UpdateCharacterInput(*cc);   // mouse-look + WASD/jump -> physics

            glm::vec3 fwd(std::cos(m_playPitch) * std::sin(m_playYaw),
                          std::sin(m_playPitch),
                         -std::cos(m_playPitch) * std::cos(m_playYaw));
            glm::vec3 center = reg.get<TransformComponent>(ce).position;   // capsule center
            float     feetY  = center.y - cc->height * 0.5f;
            glm::vec3 head(center.x, feetY + cc->eyeHeight, center.z);

            glm::vec3 eye, target;
            if (cc->view == CameraView::FirstPerson) { eye = head; target = head + fwd; }
            else { target = head; eye = head - fwd * cc->thirdDistance; }

            view       = glm::lookAt(eye, target, glm::vec3(0.0f, 1.0f, 0.0f));
            projection = glm::perspective(glm::radians(70.0f), aspect, 0.05f, 2000.0f);
        } else {
            for (auto [e, t, cam] : reg.view<TransformComponent, CameraComponent>().each()) {
                glm::mat4 camWorld = glm::translate(glm::mat4(1.0f), t.position)
                                   * glm::mat4_cast(t.rotation);
                view       = glm::inverse(camWorld);
                projection = glm::perspective(glm::radians(cam.fov), aspect, cam.nearZ, cam.farZ);
                break;
            }
        }
    }

    ImVec2 imageScreenPos = ImGui::GetCursorScreenPos();

    if (m_sceneRenderer && viewportSize.x > 0 && viewportSize.y > 0) {
        // No selection outline while playing — that's an editor-only concept.
        entt::entity selEntity = editing ? m_selected : entt::null;
        uint32_t textureID = m_sceneRenderer->Render(
            m_scene, selEntity,
            (int)viewportSize.x, (int)viewportSize.y,
            view, projection);

        if (textureID != 0)
            ImGui::Image((ImTextureID)(intptr_t)textureID, viewportSize, ImVec2(0, 1), ImVec2(1, 0));

        if (ImGui::BeginDragDropTarget()) {
            entt::registry& reg = m_scene.Reg();
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ASSET_EMAT")) {
                std::string matPath(static_cast<const char*>(p->Data));

                ImVec2 mousePos = ImGui::GetIO().MousePos;
                ImVec2 vpMin    = imageScreenPos;
                float ndcX = (2.0f * (mousePos.x - vpMin.x)) / viewportSize.x - 1.0f;
                float ndcY = 1.0f - (2.0f * (mousePos.y - vpMin.y)) / viewportSize.y;
                entt::entity hit = PickObject(m_scene, view, projection, ndcX, ndcY);

                entt::entity target = (hit != entt::null) ? hit : m_selected;
                if (m_scene.Valid(target)) {
                    reg.emplace_or_replace<MaterialComponent>(target, MaterialComponent{ matPath });
                    if (m_nodeEditor) m_nodeEditor->EnsureCompiled(matPath, m_sceneRenderer);
                    m_selected = target;
                    MarkDirty();
                    namespace fs = std::filesystem;
                    LogInfo("Material \"" + fs::path(matPath).stem().string()
                        + "\" -> " + reg.get<NameComponent>(target).name);
                }
            }

            if (ImGui::GetDragDropPayload() &&
                ImGui::GetDragDropPayload()->IsDataType("ASSET_EMAT"))
            {
                ImVec2 mousePos = ImGui::GetIO().MousePos;
                ImVec2 vpMin    = imageScreenPos;
                float ndcX = (2.0f * (mousePos.x - vpMin.x)) / viewportSize.x - 1.0f;
                float ndcY = 1.0f - (2.0f * (mousePos.y - vpMin.y)) / viewportSize.y;
                entt::entity hit = PickObject(m_scene, view, projection, ndcX, ndcY);

                entt::entity target = (hit != entt::null) ? hit : m_selected;
                ImGui::BeginTooltip();
                if (m_scene.Valid(target))
                    ImGui::Text("Assign to: %s", reg.get<NameComponent>(target).name.c_str());
                else
                    ImGui::TextDisabled("No object under cursor");
                ImGui::EndTooltip();
            }

            ImGui::EndDragDropTarget();
        }
    }

    // ── Light gizmo icons + selected-light range overlay ─────────────────
    // Lights have no mesh; draw a billboarded bulb icon so they're visible.
    // Editor-only: hidden while playing for a clean game view.
    if (editing) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        glm::mat4 viewProj = projection * view;
        constexpr float kPi = 3.14159265358979f;

        // World point → screen position; false if behind the camera.
        auto toScreen = [&](const glm::vec3& w, ImVec2& out) -> bool {
            glm::vec4 c = viewProj * glm::vec4(w, 1.0f);
            if (c.w <= 0.0001f) return false;
            glm::vec3 n = glm::vec3(c) / c.w;
            out = ImVec2(imageScreenPos.x + (n.x * 0.5f + 0.5f) * viewportSize.x,
                         imageScreenPos.y + (1.0f - (n.y * 0.5f + 0.5f)) * viewportSize.y);
            return true;
        };
        // Two orthonormal vectors spanning the plane perpendicular to n.
        auto planeBasis = [](const glm::vec3& n, glm::vec3& u, glm::vec3& v) {
            glm::vec3 a = (fabsf(n.y) < 0.99f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
            u = glm::normalize(glm::cross(a, n));
            v = glm::normalize(glm::cross(n, u));
        };
        // World-space circle drawn as projected segments (handles partial clip).
        auto worldCircle = [&](const glm::vec3& C, const glm::vec3& u, const glm::vec3& v,
                               float R, ImU32 col) {
            const int N = 48;
            ImVec2 prev; bool havePrev = false;
            for (int k = 0; k <= N; ++k) {
                float t = (float)k / N * 2.0f * kPi;
                glm::vec3 w = C + R * (cosf(t) * u + sinf(t) * v);
                ImVec2 s; bool ok = toScreen(w, s);
                if (ok && havePrev) dl->AddLine(prev, s, col, 1.2f);
                prev = s; havePrev = ok;
            }
        };

        for (auto [e, t, L] : m_scene.Reg().view<TransformComponent, LightComponent>().each()) {
            (void)t;
            glm::mat4 world = WorldMatrixOf(m_scene.Reg(), e);
            glm::vec3 pos = glm::vec3(world[3]);
            bool sel = (m_selected == e);

            ImVec2 sp;
            bool   onScreen = toScreen(pos, sp);

            // Range / cone overlay — only for the selected light, kept subtle.
            if (sel) {
                glm::vec3 dir = glm::normalize(glm::mat3(world) * glm::vec3(0.0f, 0.0f, -1.0f));
                ImU32 line = IM_COL32((int)(L.color.r * 255), (int)(L.color.g * 255),
                                      (int)(L.color.b * 255), 70);   // low alpha = subtle

                if (L.type == 1) {                            // Point → wireframe sphere
                    worldCircle(pos, glm::vec3(1,0,0), glm::vec3(0,1,0), L.range, line);
                    worldCircle(pos, glm::vec3(0,1,0), glm::vec3(0,0,1), L.range, line);
                    worldCircle(pos, glm::vec3(1,0,0), glm::vec3(0,0,1), L.range, line);
                } else if (L.type == 2) {                     // Spot → cone
                    glm::vec3 u, v; planeBasis(dir, u, v);
                    glm::vec3 base   = pos + dir * L.range;
                    float     radius = L.range * tanf(glm::radians(L.outerDeg));
                    worldCircle(base, u, v, radius, line);
                    for (int k = 0; k < 4; ++k) {             // 4 edge lines apex → rim
                        float t2 = (float)k / 4 * 2.0f * kPi;
                        glm::vec3 rim = base + radius * (cosf(t2) * u + sinf(t2) * v);
                        ImVec2 a, b;
                        if (toScreen(pos, a) && toScreen(rim, b)) dl->AddLine(a, b, line, 1.2f);
                    }
                } else {                                      // Directional → short ray
                    ImVec2 a, b;
                    if (toScreen(pos, a) && toScreen(pos + dir * 3.0f, b))
                        dl->AddLine(a, b, line, 1.5f);
                }
            }

            if (!onScreen) continue;

            ImU32  tint = IM_COL32((int)(L.color.r * 255), (int)(L.color.g * 255),
                                   (int)(L.color.b * 255), 255);
            const char* icon = (L.type == 0) ? ICON_FA_SUN : ICON_FA_LIGHTBULB;
            ImVec2 ts = ImGui::CalcTextSize(icon);
            ImVec2 tp(sp.x - ts.x * 0.5f, sp.y - ts.y * 0.5f);

            if (sel) dl->AddCircle(sp, ts.x * 0.9f, IM_COL32(255, 200, 80, 220), 0, 2.0f);
            dl->AddText(ImVec2(tp.x + 1, tp.y + 1), IM_COL32(0, 0, 0, 160), icon);  // shadow
            dl->AddText(tp, tint, icon);
        }

        // ── Collider wireframes (green, editor-only) ─────────────────────
        const ImU32 colWire = IM_COL32(96, 216, 120, 205);
        for (auto [e, col] : m_scene.Reg().view<ColliderComponent>().each()) {
            glm::mat4 world = WorldMatrixOf(m_scene.Reg(), e);
            glm::vec3 cpos = glm::vec3(world[3]);
            glm::vec3 cscl(glm::length(glm::vec3(world[0])),
                           glm::length(glm::vec3(world[1])),
                           glm::length(glm::vec3(world[2])));

            if (col.shape == ColliderShape::Box) {
                glm::vec3 h = col.halfExtents;
                ImVec2 pt[8]; bool ok[8];
                for (int i = 0; i < 8; ++i) {
                    glm::vec3 lp((i & 1) ? h.x : -h.x, (i & 2) ? h.y : -h.y, (i & 4) ? h.z : -h.z);
                    ok[i] = toScreen(glm::vec3(world * glm::vec4(lp, 1.0f)), pt[i]);
                }
                static const int edges[12][2] = {
                    {0,1},{1,3},{3,2},{2,0}, {4,5},{5,7},{7,6},{6,4}, {0,4},{1,5},{2,6},{3,7} };
                for (auto& ed : edges)
                    if (ok[ed[0]] && ok[ed[1]]) dl->AddLine(pt[ed[0]], pt[ed[1]], colWire, 1.4f);
            } else if (col.shape == ColliderShape::Sphere) {
                float r = col.radius * std::max({ cscl.x, cscl.y, cscl.z });
                worldCircle(cpos, glm::vec3(1,0,0), glm::vec3(0,1,0), r, colWire);
                worldCircle(cpos, glm::vec3(0,1,0), glm::vec3(0,0,1), r, colWire);
                worldCircle(cpos, glm::vec3(1,0,0), glm::vec3(0,0,1), r, colWire);
            } else {  // Capsule (axis-aligned approximation: two rings + side lines)
                float r  = col.radius     * std::max(cscl.x, cscl.z);
                float hh = col.halfHeight * cscl.y;
                glm::vec3 top = cpos + glm::vec3(0, hh, 0), bot = cpos - glm::vec3(0, hh, 0);
                worldCircle(top, glm::vec3(1,0,0), glm::vec3(0,0,1), r, colWire);
                worldCircle(bot, glm::vec3(1,0,0), glm::vec3(0,0,1), r, colWire);
                glm::vec3 d4[4] = { {1,0,0},{-1,0,0},{0,0,1},{0,0,-1} };
                for (auto& d : d4) {
                    ImVec2 a, b;
                    if (toScreen(top + d * r, a) && toScreen(bot + d * r, b)) dl->AddLine(a, b, colWire, 1.4f);
                }
            }
        }
    }

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetRect(imageScreenPos.x, imageScreenPos.y, viewportSize.x, viewportSize.y);

    {
        ImGuizmo::Style& gs = ImGuizmo::GetStyle();
        if (ImGuizmo::IsUsing())
            gs.Colors[ImGuizmo::SELECTION] = ImVec4(1.0f, 1.0f, 1.0f, 0.30f);
        else
            gs.Colors[ImGuizmo::SELECTION] = ImVec4(1.0f, 1.0f, 1.0f, 0.88f);
    }

    if (editing && m_scene.Valid(m_selected) &&
        EntityType(m_scene.Reg(), m_selected) != PrimitiveType::Empty) {
        entt::registry&     reg = m_scene.Reg();
        TransformComponent& tr  = reg.get<TransformComponent>(m_selected);

        // The gizmo manipulates the entity's WORLD matrix. Build it from the
        // parent's (cached) world transform and the entity's own local transform.
        glm::mat4 parentWorld(1.0f);
        if (const auto* pc = reg.try_get<ParentComponent>(m_selected);
            pc && pc->parent != entt::null && reg.valid(pc->parent))
            parentWorld = WorldMatrixOf(reg, pc->parent);

        glm::mat4 world = parentWorld * tr.Matrix();
        glm::mat4 delta(1.0f);

        if (ImGuizmo::Manipulate(
                glm::value_ptr(view), glm::value_ptr(projection),
                m_gizmoOp, m_gizmoMode,
                glm::value_ptr(world),
                glm::value_ptr(delta)))
        {
            // Convert the manipulated world matrix back into the parent's space.
            SetTransformFromMatrix(tr, glm::inverse(parentWorld) * world);
            MarkDirty();
        }
    }

    ImVec2 imagePos = ImVec2(imageScreenPos.x - ImGui::GetWindowPos().x,
                             imageScreenPos.y - ImGui::GetWindowPos().y);

    if (viewportSize.x > 0 && viewportSize.y > 0 && !ImGuizmo::IsUsing() && !ImGuizmo::IsOver()) {
        ImGui::SetCursorPos(imagePos);
        ImGui::InvisibleButton("ViewportInteraction", viewportSize,
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);

        if (charCamActive) {
            // Play + character: click in the viewport (re)captures the mouse for
            // FPS control; Esc releases it.
            if (!m_mouseCaptured && ImGui::IsItemClicked()) {
                m_mouseCaptured = true;
                glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
        } else {
            if (ImGui::IsItemHovered()) {
                float wheel = ImGui::GetIO().MouseWheel;
                if (wheel != 0.0f) m_camera.Zoom(wheel);
            }
            if (ImGui::IsItemActivated())
                m_viewportDragAccum = 0.0f;

            if (ImGui::IsItemActive()) {
                ImVec2 delta = ImGui::GetIO().MouseDelta;
                m_viewportDragAccum += std::sqrt(delta.x * delta.x + delta.y * delta.y);
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                    m_camera.OrbitDrag(delta.x, delta.y);
                else if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
                    m_camera.Pan(delta.x, delta.y);
            }

            if (editing && ImGui::IsItemDeactivated() && m_viewportDragAccum < 4.0f && !ImGuizmo::IsOver()) {
                ImVec2 itemMin  = ImGui::GetItemRectMin();
                ImVec2 mousePos = ImGui::GetIO().MousePos;
                float ndcX = (2.0f * (mousePos.x - itemMin.x)) / viewportSize.x - 1.0f;
                float ndcY = 1.0f - (2.0f * (mousePos.y - itemMin.y)) / viewportSize.y;

                entt::entity hit = PickObject(m_scene, view, projection, ndcX, ndcY);
                if (hit != entt::null) {
                    SelectEntity(hit);
                    LogInfo("Selected: " + m_scene.Reg().get<NameComponent>(hit).name);
                } else {
                    m_selected = entt::null;
                }
            }
        }
    }

    if (editing && ImGui::IsWindowFocused() && !ImGuizmo::IsUsing()) {
        if (ImGui::IsKeyPressed(ImGuiKey_M)) m_gizmoOp = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) m_gizmoOp = ImGuizmo::ROTATE;
        if (ImGui::IsKeyPressed(ImGuiKey_S)) m_gizmoOp = ImGuizmo::SCALE;
        if (ImGui::IsKeyPressed(ImGuiKey_X))
            m_gizmoMode = (m_gizmoMode == ImGuizmo::WORLD) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
    }

    ImGui::SetCursorPos(ImVec2(imagePos.x + 8.0f, imagePos.y + 8.0f));
    if (editing) {
        const char* opName = (m_gizmoOp == ImGuizmo::TRANSLATE) ? "Move (M)"  :
                             (m_gizmoOp == ImGuizmo::ROTATE)    ? "Rotate (R)":
                                                                   "Scale (S)";
        const char* modeName = (m_gizmoMode == ImGuizmo::WORLD) ? "World" : "Local";
        ImGui::TextColored(ImVec4(0.72f, 0.72f, 0.75f, 1.0f),
            "%zu objects  |  %s  |  %s (X)  |  %.0f FPS",
            (size_t)m_scene.Reg().view<NameComponent>().size(), opName, modeName, ImGui::GetIO().Framerate);
    } else {
        // Play-mode indicator: border + status label.
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 col = (m_playState == PlayState::Paused) ? IM_COL32(235, 200, 90, 230)
                                                       : IM_COL32(220, 90, 90, 230);
        dl->AddRect(imageScreenPos, ImVec2(imageScreenPos.x + viewportSize.x,
                    imageScreenPos.y + viewportSize.y), col, 0.0f, 0, 2.5f);
        const char* label = (m_playState == PlayState::Paused) ? ICON_FA_PAUSE "  PAUSED"
                                                               : ICON_FA_PLAY  "  PLAYING";
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(col), "%s   |   %.1fs   |   %.0f FPS",
                           label, m_playTime, ImGui::GetIO().Framerate);
    }

    // Timeline strip below the 3D view (placeholder for now).
    RenderViewportTimeline();

    ImGui::End();
}

void EditorUI::RenderInspector() {
    if (!m_showInspector) return;

    ImGui::Begin("Inspector", &m_showInspector, ImGuiWindowFlags_NoCollapse);

    if (m_scene.Valid(m_selected)) {
        entt::registry& reg      = m_scene.Reg();
        entt::entity    selected = m_selected;
        PrimitiveType   ptype    = EntityType(reg, selected);

        // Section header bar: full-width plate with a crimson accent tab + title.
        auto SectionHeader = [](const char* label) {
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 p = ImGui::GetCursorScreenPos();
            float  w = ImGui::GetContentRegionAvail().x;
            float  h = ImGui::GetTextLineHeight() + 10.0f;
            dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), IM_COL32(46, 46, 54, 255), 5.0f);
            dl->AddRectFilled(p, ImVec2(p.x + 3.0f, p.y + h), IM_COL32(192, 54, 76, 255),
                              5.0f, ImDrawFlags_RoundCornersLeft);
            dl->AddText(ImVec2(p.x + 12.0f, p.y + 5.0f), IM_COL32(236, 236, 242, 255), label);
            ImGui::Dummy(ImVec2(0.0f, h + 4.0f));
        };

        ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.88f, 1.0f), "Selected: %s",
                           reg.get<NameComponent>(selected).name.c_str());
        ImGui::TextDisabled("Type: %s", ToString(ptype));

        SectionHeader("Transform");
        ImGui::Indent();
        RenderTransformEditor(selected);
        ImGui::Unindent();

        if (ptype == PrimitiveType::Light) {
            SectionHeader("Light");
            ImGui::Indent();
            LightComponent& L = reg.get<LightComponent>(selected);
            bool lc = false;

            const char* kinds[] = { "Directional", "Point", "Spot" };
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##ltype", &L.type, kinds, 3)) lc = true;
            if (L.type < 0) L.type = 0;
            if (L.type > 2) L.type = 2;

            ImGui::TextDisabled("Color");
            if (ImGui::ColorEdit3("##lcol", glm::value_ptr(L.color),
                                  ImGuiColorEditFlags_NoInputs)) lc = true;

            ImGui::TextDisabled("Intensity");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat("##lint", &L.intensity, 0.05f, 0.0f, 100.0f, "%.2f")) lc = true;

            if (L.type != 0) {  // point / spot
                ImGui::TextDisabled("Range");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##lrange", &L.range, 0.1f, 0.1f, 1000.0f, "%.1f")) lc = true;
            }
            if (L.type == 2) {  // spot
                ImGui::TextDisabled("Cone (inner / outer deg)");
                ImGui::SetNextItemWidth(-1);
                float cone[2] = { L.innerDeg, L.outerDeg };
                if (ImGui::DragFloat2("##lcone", cone, 0.2f, 0.0f, 89.0f, "%.1f")) {
                    L.innerDeg = cone[0]; L.outerDeg = cone[1]; lc = true;
                }
                if (L.innerDeg > L.outerDeg) L.innerDeg = L.outerDeg;
            }

            if (lc) MarkDirty();
            ImGui::Unindent();
        }

        if (ptype == PrimitiveType::Model) {
            SectionHeader("Model Asset");
            ImGui::Indent();
            MeshComponent& M = reg.get<MeshComponent>(selected);
            ImGui::TextDisabled("%s", M.modelPath.c_str());
            if (ImGui::Button("Reimport")) {
                std::filesystem::path emdl(M.modelPath);
                std::string guessedSource = emdl.replace_extension("").string();
                for (const char* ext : { ".obj", ".fbx", ".gltf", ".glb" }) {
                    std::string candidate = guessedSource + ext;
                    if (std::filesystem::exists(candidate)) {
                        std::string newEmdl = ModelImporter::Import(candidate);
                        if (!newEmdl.empty()) {
                            glm::vec3 mn, mx;
                            if (ModelMesh::ReadAabb(newEmdl, mn, mx)) { M.aabbMin = mn; M.aabbMax = mx; }
                            LogSuccess("Reimported: " + candidate);
                        }
                        break;
                    }
                }
            }
            ImGui::Unindent();
        }

        SectionHeader("Material");
        ImGui::Indent();
        MaterialComponent* matc = reg.try_get<MaterialComponent>(selected);
        if (!matc || matc->path.empty()) {
            ImGui::TextDisabled("No material assigned");
            ImGui::TextDisabled("Drag .emat from Asset Browser");
        } else {
            namespace fs = std::filesystem;
            std::string matPath = matc->path;
            if (m_previewRenderer) {
                Material* pm = MaterialManager::GetOrLoad(matPath);
                if (pm) {
                    uint32_t pt = m_previewRenderer->GetPreview(pm, matPath);
                    if (pt) {
                        ImDrawList* dl = ImGui::GetWindowDrawList();
                        ImVec2 cp = ImGui::GetCursorScreenPos();
                        constexpr float tsz = 22.0f;
                        dl->AddImageRounded((ImTextureID)(intptr_t)pt,
                            cp, {cp.x+tsz, cp.y+tsz}, ImVec2(0,1), ImVec2(1,0),
                            IM_COL32_WHITE, 4.0f);
                        ImGui::Dummy(ImVec2(tsz, tsz));
                        ImGui::SameLine(0, 6);
                    }
                }
            }
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.0f);
            ImGui::TextDisabled("%s", fs::path(matPath).filename().string().c_str());
            ImGui::SameLine(ImGui::GetContentRegionMax().x - 20.0f);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.0f);
            if (IconButton(ICON_FA_XMARK, "##detachmat", ImVec2(20.0f, 20.0f))) {
                reg.remove<MaterialComponent>(selected);
                MarkDirty();
            }
            Material* mat = MaterialManager::GetOrLoad(matPath);
            if (mat) {
                bool changed = false;
                constexpr float kLabelW = 86.0f;

                auto Row = [&](const char* lbl, auto fn) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                    ImGui::TextDisabled("%s", lbl);
                    ImGui::TableSetColumnIndex(1);
                    if (fn()) changed = true;
                };

                if (ImGui::BeginTable("##surf", 2,
                    ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerH)) {
                    ImGui::TableSetupColumn("lbl", ImGuiTableColumnFlags_WidthFixed, kLabelW);
                    ImGui::TableSetupColumn("ctl", ImGuiTableColumnFlags_WidthStretch);

                    Row("Base Color", [&]{ return ColorSwatchEdit("##col", mat->color, true); });
                    Row("Metallic",   [&]{ return FillSlider("##met", "Metallic",  &mat->metallic,  0.0f, 1.0f); });
                    Row("Roughness",  [&]{ return FillSlider("##rou", "Roughness", &mat->roughness, 0.0f, 1.0f); });

                    ImGui::EndTable();
                }

                ImGui::Spacing();
                ImGui::TextDisabled("Emissive");
                if (ImGui::BeginTable("##emiss", 2,
                    ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerH)) {
                    ImGui::TableSetupColumn("lbl", ImGuiTableColumnFlags_WidthFixed, kLabelW);
                    ImGui::TableSetupColumn("ctl", ImGuiTableColumnFlags_WidthStretch);

                    Row("Color",     [&]{ return ColorSwatchEdit("##ecol", mat->emissiveColor, false); });
                    Row("Intensity", [&]{ return FillSlider("##ei", "Intensity", &mat->emissiveIntensity, 0.0f, 10.0f, "%.1f"); });

                    ImGui::EndTable();
                }

                if (changed) { mat->Save(matPath); m_sceneDirty = true; }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Text("Albedo Texture");

                Texture* tex = mat->albedoTexture.empty()
                    ? nullptr
                    : TextureManager::GetOrLoad(mat->albedoTexture);

                if (tex) {
                    ImGui::Image((ImTextureID)(intptr_t)tex->GetID(), ImVec2(80, 80), ImVec2(0,1), ImVec2(1,0));
                    ImGui::SameLine();
                    ImGui::BeginGroup();
                    ImGui::TextDisabled("%s",
                        std::filesystem::path(mat->albedoTexture).filename().string().c_str());
                    if (ImGui::Button("Change##tex")) {
                        std::string p = OpenTextureFileDialog();
                        if (!p.empty()) {
                            TextureManager::Invalidate(mat->albedoTexture);
                            mat->albedoTexture = p;
                            mat->Save(matPath);
                            m_sceneDirty = true;
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Remove##tex")) {
                        TextureManager::Invalidate(mat->albedoTexture);
                        mat->albedoTexture.clear();
                        mat->Save(matPath);
                        m_sceneDirty = true;
                    }
                    ImGui::EndGroup();
                } else {
                    ImGui::TextDisabled("None");
                    if (ImGui::Button("Load Texture...")) {
                        std::string p = OpenTextureFileDialog();
                        if (!p.empty()) {
                            mat->albedoTexture = p;
                            mat->Save(matPath);
                            m_sceneDirty = true;
                        }
                    }
                }

                ImGui::Spacing();
                ImGui::Text("Normal Map");
                {
                    Texture* nrmT = mat->normalTexture.empty()
                        ? nullptr : TextureManager::GetOrLoad(mat->normalTexture);
                    if (nrmT) {
                        ImGui::Image((ImTextureID)(intptr_t)nrmT->GetID(), ImVec2(64,64), ImVec2(0,1), ImVec2(1,0));
                        ImGui::SameLine();
                        ImGui::BeginGroup();
                        ImGui::TextDisabled("%s", std::filesystem::path(mat->normalTexture).filename().string().c_str());
                        if (ImGui::Button("Change##nrm")) {
                            std::string p = OpenTextureFileDialog();
                            if (!p.empty()) { TextureManager::Invalidate(mat->normalTexture); mat->normalTexture = p; mat->Save(matPath); m_sceneDirty = true; }
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Remove##nrm")) { TextureManager::Invalidate(mat->normalTexture); mat->normalTexture.clear(); mat->Save(matPath); m_sceneDirty = true; }
                        ImGui::EndGroup();
                    } else {
                        if (ImGui::Button("Load Normal Map...")) {
                            std::string p = OpenTextureFileDialog();
                            if (!p.empty()) { mat->normalTexture = p; mat->Save(matPath); m_sceneDirty = true; }
                        }
                    }
                }

                ImGui::Spacing();
                ImGui::Text("ORM Texture");
                ImGui::TextDisabled("R=Occlusion  G=Roughness  B=Metallic");
                {
                    Texture* mrT = mat->ormTexture.empty()
                        ? nullptr : TextureManager::GetOrLoad(mat->ormTexture);
                    if (mrT) {
                        ImGui::Image((ImTextureID)(intptr_t)mrT->GetID(), ImVec2(64,64), ImVec2(0,1), ImVec2(1,0));
                        ImGui::SameLine();
                        ImGui::BeginGroup();
                        ImGui::TextDisabled("%s",
                            std::filesystem::path(mat->ormTexture).filename().string().c_str());
                        if (ImGui::Button("Change##mr")) {
                            std::string p = OpenTextureFileDialog();
                            if (!p.empty()) {
                                TextureManager::Invalidate(mat->ormTexture);
                                mat->ormTexture = p;
                                mat->Save(matPath);
                                m_sceneDirty = true;
                            }
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Remove##mr")) {
                            TextureManager::Invalidate(mat->ormTexture);
                            mat->ormTexture.clear();
                            mat->Save(matPath);
                            m_sceneDirty = true;
                        }
                        ImGui::EndGroup();
                    } else {
                        if (ImGui::Button("Load MR Texture...")) {
                            std::string p = OpenTextureFileDialog();
                            if (!p.empty()) {
                                mat->ormTexture = p;
                                mat->Save(matPath);
                                m_sceneDirty = true;
                            }
                        }
                    }
                }

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    if (m_nodeEditor)
                        m_nodeEditor->Open(matPath, m_previewRenderer, m_sceneRenderer);
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Double-click to open Node Editor");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "File not found");
                if (ImGui::Button("Clear")) {
                    reg.remove<MaterialComponent>(selected);
                    MarkDirty();
                }
            }
        }
        ImGui::Unindent();

        // ── Components ──────────────────────────────────────────────────
        SectionHeader("Components");
        ImGui::Indent();

        // Shared header row: enable toggle + name + remove X. Returns true when
        // the user clicked the X this frame (caller removes after using fields).
        auto compHeader = [&](const char* label, bool* enabled) -> bool {
            bool en = *enabled;
            if (ImGui::Checkbox("##en", &en)) { *enabled = en; MarkDirty(); }
            ImGui::SameLine();
            ImGui::Text("%s", label);
            ImGui::SameLine(ImGui::GetContentRegionMax().x - 22.0f);
            return IconButton(ICON_FA_XMARK, "##rmcomp", ImVec2(20.0f, 20.0f));
        };

        if (auto* r = reg.try_get<RotatorComponent>(selected)) {
            ImGui::PushID("rotator");
            bool remove = compHeader("Rotator", &r->enabled);
            if (!remove) {
                ImGui::TextDisabled("Axis");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat3("##axis", glm::value_ptr(r->axis), 0.01f, -1.0f, 1.0f, "%.2f")) MarkDirty();
                ImGui::TextDisabled("Speed (deg/s)");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##spd", &r->speed, 0.5f, -720.0f, 720.0f, "%.1f")) MarkDirty();
            }
            ImGui::Spacing();
            ImGui::PopID();
            if (remove) { reg.remove<RotatorComponent>(selected); MarkDirty(); }
        }

        if (auto* f = reg.try_get<FloaterComponent>(selected)) {
            ImGui::PushID("floater");
            bool remove = compHeader("Floater", &f->enabled);
            if (!remove) {
                ImGui::TextDisabled("Axis");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat3("##axis", glm::value_ptr(f->axis), 0.01f, -1.0f, 1.0f, "%.2f")) MarkDirty();
                ImGui::TextDisabled("Speed (rad/s)");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##spd", &f->speed, 0.5f, -720.0f, 720.0f, "%.1f")) MarkDirty();
                ImGui::TextDisabled("Amplitude");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##amp", &f->amount, 0.01f, 0.0f, 100.0f, "%.2f")) MarkDirty();
            }
            ImGui::Spacing();
            ImGui::PopID();
            if (remove) { reg.remove<FloaterComponent>(selected); MarkDirty(); }
        }

        if (auto* s = reg.try_get<ScriptComponent>(selected)) {
            ImGui::PushID("script");
            bool remove = compHeader("Script", &s->enabled);
            if (!remove) {
                ImGui::TextDisabled("Lua Script");
                ImGui::TextWrapped("%s", s->path.empty() ? "(none)" : s->path.c_str());
                if (ImGui::Button(ICON_FA_FOLDER_OPEN "  Browse##scr", ImVec2(-1, 0))) {
                    std::string p = OpenScriptFileDialog();
                    if (!p.empty()) {
                        namespace fs = std::filesystem;
                        std::error_code ec;
                        fs::path rel = fs::relative(p, fs::current_path(), ec);
                        std::string relStr = ec ? "" : rel.generic_string();
                        s->path = (!relStr.empty() && relStr.rfind("..", 0) != 0) ? relStr : p;
                        for (auto& ch : s->path) if (ch == '\\') ch = '/';
                        MarkDirty();
                    }
                }
                if (!s->path.empty() &&
                    ImGui::Button(ICON_FA_XMARK "  Clear##scr", ImVec2(-1, 0))) {
                    s->path.clear(); MarkDirty();
                }
            }
            ImGui::Spacing();
            ImGui::PopID();
            if (remove) { reg.remove<ScriptComponent>(selected); MarkDirty(); }
        }

        if (auto* col = reg.try_get<ColliderComponent>(selected)) {
            ImGui::PushID("collider");
            ImGui::Text("Collider");
            ImGui::SameLine(ImGui::GetContentRegionMax().x - 22.0f);
            bool remove = IconButton(ICON_FA_XMARK, "##rmcol", ImVec2(20.0f, 20.0f));
            if (!remove) {
                const char* shapes[] = { "Box", "Sphere", "Capsule" };
                int sh = (int)col->shape;
                ImGui::TextDisabled("Shape");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::Combo("##colshape", &sh, shapes, 3)) { col->shape = (ColliderShape)sh; MarkDirty(); }
                if (col->shape == ColliderShape::Box) {
                    ImGui::TextDisabled("Half Extents");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat3("##colhe", glm::value_ptr(col->halfExtents), 0.01f, 0.01f, 1000.0f, "%.2f")) MarkDirty();
                } else if (col->shape == ColliderShape::Sphere) {
                    ImGui::TextDisabled("Radius");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##colr", &col->radius, 0.01f, 0.01f, 1000.0f, "%.2f")) MarkDirty();
                } else {
                    ImGui::TextDisabled("Radius");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##colcr", &col->radius, 0.01f, 0.01f, 1000.0f, "%.2f")) MarkDirty();
                    ImGui::TextDisabled("Half Height");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##colhh", &col->halfHeight, 0.01f, 0.01f, 1000.0f, "%.2f")) MarkDirty();
                }
            }
            ImGui::Spacing();
            ImGui::PopID();
            if (remove) { reg.remove<ColliderComponent>(selected); MarkDirty(); }
        }

        if (auto* rbp = reg.try_get<RigidBodyComponent>(selected)) {
            ImGui::PushID("rigidbody");
            ImGui::Text("Rigid Body");
            ImGui::SameLine(ImGui::GetContentRegionMax().x - 22.0f);
            bool remove = IconButton(ICON_FA_XMARK, "##rmrb", ImVec2(20.0f, 20.0f));
            if (!remove) {
                const char* types[] = { "Static", "Dynamic", "Kinematic" };
                int ty = (int)rbp->type;
                ImGui::TextDisabled("Type");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::Combo("##rbtype", &ty, types, 3)) { rbp->type = (BodyType)ty; MarkDirty(); }
                if (rbp->type == BodyType::Dynamic) {
                    ImGui::TextDisabled("Mass (kg)");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##rbmass", &rbp->mass, 0.05f, 0.001f, 1000.0f, "%.3f")) MarkDirty();
                }
                ImGui::TextDisabled("Friction");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##rbfric", &rbp->friction, 0.01f, 0.0f, 2.0f, "%.2f")) MarkDirty();
                ImGui::TextDisabled("Restitution");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##rbrest", &rbp->restitution, 0.01f, 0.0f, 1.0f, "%.2f")) MarkDirty();
                if (rbp->type == BodyType::Dynamic) {
                    bool aw = rbp->startAwake;
                    if (ImGui::Checkbox("Start Awake", &aw)) { rbp->startAwake = aw; MarkDirty(); }
                }
                if (!reg.all_of<ColliderComponent>(selected))
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Needs a Collider to simulate");
            }
            ImGui::Spacing();
            ImGui::PopID();
            if (remove) { reg.remove<RigidBodyComponent>(selected); MarkDirty(); }
        }

        if (auto* chc = reg.try_get<CharacterControllerComponent>(selected)) {
            ImGui::PushID("charctrl");
            ImGui::Text("Character Controller");
            ImGui::SameLine(ImGui::GetContentRegionMax().x - 22.0f);
            bool remove = IconButton(ICON_FA_XMARK, "##rmchar", ImVec2(20.0f, 20.0f));
            if (!remove) {
                const char* views[] = { "First Person", "Third Person" };
                int vw = (int)chc->view;
                ImGui::TextDisabled("Camera");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::Combo("##charview", &vw, views, 2)) { chc->view = (CameraView)vw; MarkDirty(); }
                ImGui::TextDisabled("Height / Radius");
                ImGui::SetNextItemWidth(-1);
                float hr[2] = { chc->height, chc->radius };
                if (ImGui::DragFloat2("##charhr", hr, 0.01f, 0.1f, 10.0f, "%.2f")) {
                    chc->height = hr[0]; chc->radius = hr[1]; MarkDirty();
                }
                ImGui::TextDisabled("Move Speed");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##charspd", &chc->moveSpeed, 0.1f, 0.0f, 50.0f, "%.1f")) MarkDirty();
                ImGui::TextDisabled("Jump Speed");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##charjmp", &chc->jumpSpeed, 0.1f, 0.0f, 50.0f, "%.1f")) MarkDirty();
                ImGui::TextDisabled("Eye Height");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##chareye", &chc->eyeHeight, 0.01f, 0.0f, 10.0f, "%.2f")) MarkDirty();
                if (chc->view == CameraView::ThirdPerson) {
                    ImGui::TextDisabled("Camera Distance");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##chardist", &chc->thirdDistance, 0.05f, 0.5f, 20.0f, "%.2f")) MarkDirty();
                }
                ImGui::TextDisabled("Mouse Sensitivity");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##charsens", &chc->mouseSensitivity, 0.0001f, 0.0005f, 0.02f, "%.4f")) MarkDirty();
                ImGui::TextDisabled("Play: WASD + mouse, Space = jump, Esc = free cursor");
            }
            ImGui::Spacing();
            ImGui::PopID();
            if (remove) { reg.remove<CharacterControllerComponent>(selected); MarkDirty(); }
        }

        if (ImGui::Button(ICON_FA_PLUS "  Add Component", ImVec2(-1, 0)))
            ImGui::OpenPopup("##addcomp");
        if (ImGui::BeginPopup("##addcomp")) {
            if (!reg.all_of<RotatorComponent>(selected) && ImGui::MenuItem("Rotator")) {
                reg.emplace<RotatorComponent>(selected); MarkDirty();
            }
            if (!reg.all_of<FloaterComponent>(selected) && ImGui::MenuItem("Floater")) {
                reg.emplace<FloaterComponent>(selected); MarkDirty();
            }
            if (!reg.all_of<ScriptComponent>(selected) && ImGui::MenuItem("Script")) {
                reg.emplace<ScriptComponent>(selected); MarkDirty();
            }
            if (!reg.all_of<ColliderComponent>(selected) && ImGui::MenuItem("Collider")) {
                reg.emplace<ColliderComponent>(selected); MarkDirty();
            }
            if (!reg.all_of<RigidBodyComponent>(selected) && ImGui::MenuItem("Rigid Body")) {
                reg.emplace<RigidBodyComponent>(selected); MarkDirty();
            }
            if (!reg.all_of<CharacterControllerComponent>(selected) && ImGui::MenuItem("Character Controller")) {
                reg.emplace<CharacterControllerComponent>(selected); MarkDirty();
            }
            ImGui::EndPopup();
        }
        ImGui::Unindent();
    }
    else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No object selected");
        ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f),
                           "Select an object in the Scene Objects panel");
    }

    ImGui::End();
}

void EditorUI::RenderTransformEditor(entt::entity e) {
    if (!m_scene.Valid(e)) return;
    TransformComponent& tr = m_scene.Reg().get<TransformComponent>(e);

    auto XYZRow = [&](const char* rowLabel, float v[3], float step,
                       float vmin, float vmax, const char* fmt) -> bool
    {
        bool  changed = false;
        float gap     = ImGui::GetStyle().ItemSpacing.x;
        float h       = ImGui::GetFrameHeight();
        float fieldW  = (ImGui::GetContentRegionAvail().x - gap * 2.0f) / 3.0f;

        ImGui::TextDisabled("%s", rowLabel);

        ImVec2 rowOrigin = ImGui::GetCursorScreenPos();
        for (int i = 0; i < 3; ++i) {
            ImGui::SetCursorScreenPos({rowOrigin.x + i * (fieldW + gap), rowOrigin.y});
            char fid[32]; snprintf(fid, sizeof(fid), "##%s%d", rowLabel, i);
            if (FieldSpinner(fid, &v[i], step, vmin, vmax, fmt, fieldW)) changed = true;
        }
        ImGui::SetCursorScreenPos({rowOrigin.x, rowOrigin.y + h + gap});
        return changed;
    };

    float pos[3] = { tr.position.x, tr.position.y, tr.position.z };
    if (XYZRow("Position", pos, 0.1f, -1e6f, 1e6f, "%.2f")) {
        tr.position = glm::vec3(pos[0], pos[1], pos[2]);
        MarkDirty();
    }

    ImGui::Spacing();

    glm::vec3 euler = glm::degrees(glm::eulerAngles(tr.rotation));
    float eulerArr[3] = { euler.x, euler.y, euler.z };
    if (XYZRow("Rotation", eulerArr, 0.5f, -360.0f, 360.0f, "%.1f")) {
        tr.rotation = glm::quat(glm::radians(glm::vec3(eulerArr[0], eulerArr[1], eulerArr[2])));
        MarkDirty();
    }

    ImGui::Spacing();

    float scl[3] = { tr.scale.x, tr.scale.y, tr.scale.z };
    if (XYZRow("Scale", scl, 0.05f, 0.001f, 1e4f, "%.3f")) {
        tr.scale = glm::vec3(scl[0], scl[1], scl[2]);
        MarkDirty();
    }
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
        case ConsoleMessage::Info:    color = ImVec4(0.72f, 0.72f, 0.75f, 1.0f); tag = "INFO "; break;
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

// ── Multi-scene documents ────────────────────────────────────────────────────
SceneDoc* EditorUI::FindDoc(uint32_t id) {
    for (auto& d : m_docs) if (d.id == id) return &d;
    return nullptr;
}

SceneDoc& EditorUI::ActiveDoc() {
    if (SceneDoc* d = FindDoc(m_activeDoc)) return *d;
    if (m_docs.empty()) NewDocument();
    m_activeDoc = m_docs.front().id;
    return m_docs.front();
}

bool EditorUI::AnyDocDirty() const {
    for (const auto& d : m_docs) if (d.dirty) return true;
    return false;
}

uint32_t EditorUI::NewDocument(const std::string& name) {
    SceneDoc doc;
    doc.id   = m_nextDocId++;
    doc.name = name;
    uint32_t id = doc.id;
    m_docs.push_back(std::move(doc));        // its scene/undo start empty

    if (m_docs.size() == 1) {
        m_activeDoc = id;                    // first doc: adopt the empty working set
        ClearUndoHistory();
    } else {
        SwitchTo(id);                        // stash current, make the new empty doc active
    }
    return id;
}

void EditorUI::SwitchTo(uint32_t id) {
    if (id == m_activeDoc) return;
    if (!FindDoc(id))      return;
    if (m_playState != PlayState::Editing) ExitPlayMode();   // switching stops the game

    // Stash the live working set into the currently-active document.
    if (SceneDoc* cur = FindDoc(m_activeDoc)) {
        cur->scene            = std::move(m_scene);
        cur->selected         = m_selected;
        cur->undoStack        = std::move(m_undoStack);
        cur->redoStack        = std::move(m_redoStack);
        cur->baseline         = std::move(m_baseline);
        cur->sceneRevision    = m_sceneRevision;
        cur->baselineRevision = m_baselineRevision;
    }
    // Load the target document's state into the live working set.
    SceneDoc* nxt = FindDoc(id);
    m_scene            = std::move(nxt->scene);
    m_selected         = nxt->selected;
    m_undoStack        = std::move(nxt->undoStack);
    m_redoStack        = std::move(nxt->redoStack);
    m_baseline         = std::move(nxt->baseline);
    m_sceneRevision    = nxt->sceneRevision;
    m_baselineRevision = nxt->baselineRevision;
    m_activeDoc        = id;
}

void EditorUI::OpenScene(const std::string& path) {
    // If it's already open, just switch to its tab.
    if (!path.empty())
        for (auto& d : m_docs)
            if (d.path == path) { SwitchTo(d.id); return; }

    Scene loaded;
    if (!SceneSerializer::Load(path, loaded)) {
        LogError("Failed to load scene: " + path);
        return;
    }
    uint32_t id = NewDocument(std::filesystem::path(path).stem().string());  // new active doc
    m_scene    = std::move(loaded);                                          // becomes its scene
    m_selected = entt::null;
    if (SceneDoc* doc = FindDoc(id)) { doc->path = path; doc->dirty = false; }
    ClearUndoHistory();                 // baseline = the freshly loaded scene
    m_sceneDirty = AnyDocDirty();
    LogSuccess("Scene loaded: " + path);
}

bool EditorUI::SaveDocument(uint32_t id, bool saveAs) {
    SceneDoc* doc = FindDoc(id);
    if (!doc) return false;

    std::string path = doc->path;
    if (saveAs || path.empty()) {
        path = SaveSceneFileDialog();
        if (path.empty()) return false;
    }
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());

    // The active document's scene is live in m_scene; inactive ones are stashed.
    Scene& sc = (id == m_activeDoc) ? m_scene : doc->scene;
    if (!SceneSerializer::Save(path, sc)) {
        LogError("Failed to save scene: " + path);
        return false;
    }
    AssetDatabase::GetOrCreateGuid(path, "Scene");
    doc->path    = path;
    doc->name    = std::filesystem::path(path).stem().string();
    doc->dirty   = false;
    m_sceneDirty = AnyDocDirty();
    LogSuccess("Scene saved: " + path);
    return true;
}

void EditorUI::CloseDocument(uint32_t id) {
    if (!FindDoc(id)) return;

    // Last document: reset it to an empty Untitled instead of removing it, so the
    // editor always has one active scene.
    if (m_docs.size() == 1) {
        m_scene    = Scene();
        m_selected = entt::null;
        if (SceneDoc* d = FindDoc(id)) { d->path.clear(); d->name = "Untitled"; d->dirty = false; }
        ClearUndoHistory();
        m_sceneDirty = false;
        LogInfo("Scene closed");
        return;
    }

    // If closing the active tab, switch to a neighbour first so the live working
    // set is valid; the closed doc then owns its entities in its stashed scene.
    if (id == m_activeDoc) {
        uint32_t other = 0;
        for (auto& d : m_docs) if (d.id != id) { other = d.id; break; }
        SwitchTo(other);
    }
    m_docs.erase(std::remove_if(m_docs.begin(), m_docs.end(),
                 [id](const SceneDoc& d) { return d.id == id; }), m_docs.end());
    m_sceneDirty = AnyDocDirty();
    LogInfo("Scene closed");
}

bool EditorUI::SaveCurrentScene() {
    return SaveDocument(m_activeDoc);
}

// ── Projects ─────────────────────────────────────────────────────────────────
bool EditorUI::LoadProject(const std::string& fcprojPath) {
    namespace fs = std::filesystem;
    projects::Info info;
    if (!projects::Parse(fcprojPath, info)) {
        LogError("Cannot open project: " + fcprojPath);
        return false;
    }

    Project p;
    p.file         = fs::absolute(fcprojPath).string();
    p.dir          = fs::path(p.file).parent_path().string();
    p.name         = info.name;
    p.assetSub     = info.assetSub;
    p.startupScene = info.startupScene;
    p.assetDir     = (fs::path(p.dir) / p.assetSub).string();
    std::error_code ec; fs::create_directories(p.assetDir, ec);

    m_project = p;

    // Root the asset pipeline at the project.
    m_assetBrowserPath  = p.assetDir;
    m_assetBrowserDirty = true;
    AssetDatabase::ScanFolder(p.assetDir);
    projects::AddRecent(p.file);

    // Start from a clean single scene (or the project's startup scene).
    m_docs.clear();
    m_activeDoc = 0; m_nextDocId = 1;
    m_scene = Scene(); m_selected = entt::null;
    m_undoStack.clear(); m_redoStack.clear(); m_baseline = SceneSnapshot();
    m_sceneRevision = 0; m_baselineRevision = 0;
    m_sceneDirty = false;
    NewDocument("Untitled");
    if (!p.startupScene.empty()) {
        std::string scenePath = (fs::path(p.dir) / p.startupScene).string();
        if (fs::exists(scenePath)) OpenScene(scenePath);
    }

    LogSuccess("Project loaded: " + p.name);
    return true;
}

bool EditorUI::CreateProject(const std::string& fcprojPath, const std::string& name) {
    if (!projects::Create(fcprojPath, name)) {
        LogError("Cannot create project: " + fcprojPath);
        return false;
    }
    return LoadProject(fcprojPath);
}

// ── Undo/redo ───────────────────────────────────────────────────────────────
void EditorUI::MarkDirty() {
    m_sceneDirty = true;
    ++m_sceneRevision;
    if (SceneDoc* a = FindDoc(m_activeDoc)) a->dirty = true;  // edits target the active scene
}

SceneSnapshot EditorUI::CaptureSnapshot() {
    SceneSnapshot snap;
    std::unordered_map<entt::entity, entt::entity> map;
    CloneScene(m_scene, snap.scene, map);
    auto it = map.find(m_selected);
    snap.selected = (m_scene.Valid(m_selected) && it != map.end()) ? it->second : entt::null;
    return snap;
}

void EditorUI::RestoreSnapshot(const SceneSnapshot& snap) {
    std::unordered_map<entt::entity, entt::entity> map;
    CloneScene(snap.scene, m_scene, map);
    auto it = map.find(snap.selected);
    m_selected = (it != map.end()) ? it->second : entt::null;
}

void EditorUI::CommitEdit() {
    m_undoStack.push_back(std::move(m_baseline));
    if (m_undoStack.size() > kMaxUndo) m_undoStack.erase(m_undoStack.begin());
    m_redoStack.clear();
    m_baseline         = CaptureSnapshot();
    m_baselineRevision = m_sceneRevision;
}

void EditorUI::UpdateUndoCoalescing() {
    // A gesture (drag, gizmo, button press) keeps an item active; once it ends
    // and the scene revision moved, commit exactly one undo step for it.
    const bool interacting = ImGui::IsAnyItemActive() || ImGuizmo::IsUsing();
    if (!interacting && m_sceneRevision != m_baselineRevision)
        CommitEdit();
}

void EditorUI::Undo() {
    if (m_undoStack.empty()) { LogInfo("Nothing to undo"); return; }
    m_redoStack.push_back(std::move(m_baseline));   // current settled state -> redo
    RestoreSnapshot(m_undoStack.back());
    m_undoStack.pop_back();
    m_baseline         = CaptureSnapshot();
    m_baselineRevision = m_sceneRevision;           // keep equal: no spurious commit
    m_sceneDirty       = true;
    LogInfo("Undo");
}

void EditorUI::Redo() {
    if (m_redoStack.empty()) { LogInfo("Nothing to redo"); return; }
    m_undoStack.push_back(std::move(m_baseline));
    RestoreSnapshot(m_redoStack.back());
    m_redoStack.pop_back();
    m_baseline         = CaptureSnapshot();
    m_baselineRevision = m_sceneRevision;
    m_sceneDirty       = true;
    LogInfo("Redo");
}

void EditorUI::ClearUndoHistory() {
    m_undoStack.clear();
    m_redoStack.clear();
    m_baseline         = CaptureSnapshot();
    m_baselineRevision = m_sceneRevision;
}

void EditorUI::RequestClose() {
    // Explicit close (custom X / Exit menu): set during a frame, so we can't
    // rely on the top-of-frame should-close intercept (the main loop would exit
    // first). Decide here: prompt if dirty, otherwise close.
    if (m_sceneDirty) m_showUnsavedChangesDialog = true;
    else              glfwSetWindowShouldClose(m_window, 1);
}

void EditorUI::RenderUnsavedChangesDialog() {
    if (m_showUnsavedChangesDialog)
        ImGui::OpenPopup("Unsaved Changes");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500.0f, 230.0f), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(28, 24));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,  6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,    ImVec2(8, 12));

    if (ImGui::BeginPopupModal("Unsaved Changes", nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar)) {

        // Centered title
        ImGui::SetWindowFontScale(1.25f);
        {
            const char* title = "Unsaved Changes";
            float tw = ImGui::CalcTextSize(title).x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                                 (ImGui::GetContentRegionAvail().x - tw) * 0.5f);
            ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.97f, 1.0f), "%s", title);
        }
        ImGui::SetWindowFontScale(1.0f);

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.72f, 0.72f, 0.76f, 1.0f),
            "You have unsaved changes to the scene.");
        ImGui::TextColored(ImVec4(0.72f, 0.72f, 0.76f, 1.0f),
            "Do you want to save before closing?");

        // Push the button row to the bottom of the dialog.
        const float bw      = 112.0f;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        float rowLeft = ImGui::GetCursorPosX();
        float fullW   = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() -
                             ImGui::GetStyle().WindowPadding.y - ImGui::GetFrameHeight());

        // Save — accent (crimson), pinned left
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(192, 54, 76, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(214, 68, 92, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(168, 44, 64, 255));
        if (ImGui::Button("Save", ImVec2(bw, 0))) {
            // Save every dirty document; close only if none was cancelled.
            std::vector<uint32_t> dirtyIds;
            for (const auto& d : m_docs) if (d.dirty) dirtyIds.push_back(d.id);
            bool allSaved = true;
            for (uint32_t id : dirtyIds)
                if (!SaveDocument(id)) { allSaved = false; break; }
            if (allSaved) glfwSetWindowShouldClose(m_window, 1);
            m_showUnsavedChangesDialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(3);

        // Don't Save + Cancel — pinned right
        ImGui::SameLine();
        ImGui::SetCursorPosX(rowLeft + fullW - (bw * 2.0f + spacing));
        if (ImGui::Button("Don't Save", ImVec2(bw, 0))) {
            m_showUnsavedChangesDialog = false;
            ImGui::CloseCurrentPopup();
            glfwSetWindowShouldClose(m_window, 1);
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(bw, 0))) {
            m_showUnsavedChangesDialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(4);
}

void EditorUI::RequestCloseDocument(uint32_t id) {
    SceneDoc* doc = FindDoc(id);
    if (!doc) return;
    if (doc->dirty) {
        m_closePromptDoc = id;              // confirm before discarding
    } else {
        CloseDocument(id);
        m_tabSelectId = m_activeDoc;
    }
}

void EditorUI::RenderCloseSceneDialog() {
    if (m_closePromptDoc == 0) return;
    SceneDoc* doc = FindDoc(m_closePromptDoc);
    if (!doc) { m_closePromptDoc = 0; return; }
    std::string sceneName = doc->name.empty() ? "Untitled" : doc->name;

    ImGui::OpenPopup("Close Scene");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500.0f, 230.0f), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(28, 24));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,  6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,    ImVec2(8, 12));

    if (ImGui::BeginPopupModal("Close Scene", nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar)) {

        ImGui::SetWindowFontScale(1.25f);
        {
            const char* title = "Close Scene";
            float tw = ImGui::CalcTextSize(title).x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                                 (ImGui::GetContentRegionAvail().x - tw) * 0.5f);
            ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.97f, 1.0f), "%s", title);
        }
        ImGui::SetWindowFontScale(1.0f);

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.72f, 0.72f, 0.76f, 1.0f),
            "\"%s\" has unsaved changes.", sceneName.c_str());
        ImGui::TextColored(ImVec4(0.72f, 0.72f, 0.76f, 1.0f),
            "Do you want to save before closing?");

        const float bw      = 112.0f;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        float rowLeft = ImGui::GetCursorPosX();
        float fullW   = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() -
                             ImGui::GetStyle().WindowPadding.y - ImGui::GetFrameHeight());

        uint32_t id = m_closePromptDoc;
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(192, 54, 76, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(214, 68, 92, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(168, 44, 64, 255));
        if (ImGui::Button("Save", ImVec2(bw, 0))) {
            if (SaveDocument(id)) { CloseDocument(id); m_tabSelectId = m_activeDoc; }
            m_closePromptDoc = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        ImGui::SetCursorPosX(rowLeft + fullW - (bw * 2.0f + spacing));
        if (ImGui::Button("Don't Save", ImVec2(bw, 0))) {
            CloseDocument(id);
            m_tabSelectId    = m_activeDoc;
            m_closePromptDoc = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(bw, 0))) {
            m_closePromptDoc = 0;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(4);
}

void EditorUI::AddGameObject(const std::string& name, PrimitiveType type) {
    std::string fullName = name + "_" + std::to_string(m_objectCounter++);
    entt::entity e = m_scene.Create(fullName);
    entt::registry& reg = m_scene.Reg();

    MeshKind mk = MeshKindFromPrimitive(type);
    if (mk != MeshKind::None)               reg.emplace<MeshComponent>(e, MeshComponent{ mk });
    else if (type == PrimitiveType::Light)  reg.emplace<LightComponent>(e);
    else if (type == PrimitiveType::Camera) reg.emplace<CameraComponent>(e);

    MarkDirty();
    LogInfo("Created: " + fullName);
}

void EditorUI::AddModelObject(const std::string& emdlPath) {
    std::string fullName = std::filesystem::path(emdlPath).stem().string()
                         + "_" + std::to_string(m_objectCounter++);
    entt::entity e = m_scene.Create(fullName);

    MeshComponent mc;
    mc.kind      = MeshKind::Model;
    mc.modelPath = emdlPath;
    glm::vec3 mn, mx;
    if (ModelMesh::ReadAabb(emdlPath, mn, mx)) { mc.aabbMin = mn; mc.aabbMax = mx; }
    m_scene.Reg().emplace<MeshComponent>(e, mc);

    MarkDirty();
    LogSuccess("Model added: " + fullName);
}

void EditorUI::ImportModelAsync(const std::string& sourcePath) {
    std::string name = std::filesystem::path(sourcePath).filename().string();
    LogInfo("Importing in background: " + name);
    jobs::JobSystem::Get().Submit([this, sourcePath, name]() {
        // Worker thread: assimp parse + .emdl/.meta write (no GL, no registry).
        std::string emdl = ModelImporter::Import(sourcePath);
        // Marshal the result to the main thread (registry write + lazy GL upload).
        jobs::JobSystem::Get().EnqueueMainThread([this, emdl, name]() {
            if (!emdl.empty()) {
                AddModelObject(emdl);
                m_assetBrowserDirty = true;
            } else {
                LogError("Import failed: " + name);
            }
        });
    });
}

void EditorUI::SelectEntity(entt::entity e) {
    if (m_scene.Valid(e)) m_selected = e;
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
