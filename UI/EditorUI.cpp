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
#include "../Assets/ModelImporter.h"
#include "../Renderer/SceneRenderer.h"
#include "../Renderer/ModelMesh.h"
#include "../Renderer/Material.h"
#include "../Renderer/MaterialManager.h"
#include "../Renderer/MaterialPreviewRenderer.h"
#include "../Renderer/TextureManager.h"
#include "../Renderer/Picking.h"
#include "../Scene/Transform.h"
#include "ImGuizmo.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <algorithm>
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

    std::string OpenSceneFileDialog() {
        OPENFILENAMEA ofn{};
        char fileName[MAX_PATH] = "";
        ofn.lStructSize  = sizeof(ofn);
        ofn.lpstrFile    = fileName;
        ofn.nMaxFile     = MAX_PATH;
        ofn.lpstrFilter  = "Engine Scene\0*.escn\0All Files\0*.*\0";
        ofn.lpstrDefExt  = "escn";
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
        ofn.lpstrFilter  = "Engine Scene\0*.escn\0All Files\0*.*\0";
        ofn.lpstrDefExt  = "escn";
        ofn.nFilterIndex = 1;
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

    // When a material is renamed in the node editor, re-point objects + UI.
    m_nodeEditor->SetRenameHandler([this](const std::string& oldP, const std::string& newP) {
        for (auto& obj : m_gameObjects)
            if (obj.materialPath == oldP) obj.materialPath = newP;
        if (m_assetBrowserSelected == oldP) m_assetBrowserSelected = newP;
        m_compiledGraphs.erase(oldP);          // re-precompile under the new path
        m_assetBrowserDirty = true;
        m_sceneDirty = true;
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

    std::filesystem::create_directories("Assets");
    AssetDatabase::ScanFolder("Assets");

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_CheckMark] = ImVec4(1.0f, 1.0f, 1.0f, 0.90f);

    style.WindowRounding    = 0.0f;
    style.ChildRounding     = 6.0f;
    style.FrameRounding     = 4.0f;
    style.PopupRounding     = 6.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding      = 4.0f;
    style.TabRounding       = 5.0f;

    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.ChildBorderSize   = 1.0f;

    style.WindowPadding     = ImVec2(12.0f, 10.0f);
    style.FramePadding      = ImVec2(8.0f,  5.0f);
    style.ItemSpacing       = ImVec2(8.0f,  6.0f);
    style.ItemInnerSpacing  = ImVec2(6.0f,  6.0f);
    style.IndentSpacing     = 16.0f;
    style.ScrollbarSize     = 11.0f;
    style.GrabMinSize       = 10.0f;
    style.WindowMenuButtonPosition = ImGuiDir_None;

    const ImVec4 bg0    = ImVec4(0.118f, 0.118f, 0.118f, 1.00f);
    const ImVec4 bg1    = ImVec4(0.145f, 0.145f, 0.149f, 1.00f);
    const ImVec4 bg2    = ImVec4(0.235f, 0.235f, 0.235f, 1.00f);
    const ImVec4 bg3    = ImVec4(0.282f, 0.282f, 0.282f, 1.00f);
    const ImVec4 bg4    = ImVec4(0.322f, 0.322f, 0.322f, 1.00f);
    const ImVec4 popup  = ImVec4(0.157f, 0.157f, 0.161f, 1.00f);
    const ImVec4 acc    = ImVec4(0.753f, 0.212f, 0.298f, 1.00f);
    const ImVec4 accH   = ImVec4(0.816f, 0.267f, 0.369f, 1.00f);
    const ImVec4 accA   = ImVec4(0.659f, 0.157f, 0.251f, 1.00f);
    const ImVec4 accDim = ImVec4(0.753f, 0.212f, 0.298f, 0.22f);
    const ImVec4 txt    = ImVec4(0.800f, 0.800f, 0.800f, 1.00f);
    const ImVec4 txtD   = ImVec4(0.522f, 0.522f, 0.522f, 1.00f);
    const ImVec4 brd    = ImVec4(1.000f, 1.000f, 1.000f, 0.10f);
    const ImVec4 brdS   = ImVec4(0.000f, 0.000f, 0.000f, 0.00f);

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text]                 = txt;
    c[ImGuiCol_TextDisabled]         = txtD;
    c[ImGuiCol_WindowBg]             = bg0;
    c[ImGuiCol_ChildBg]              = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_PopupBg]              = popup;
    c[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.0f, 0.0f, 0.0f, 0.62f);
    c[ImGuiCol_Border]               = brd;
    c[ImGuiCol_BorderShadow]         = brdS;
    c[ImGuiCol_FrameBg]              = bg2;
    c[ImGuiCol_FrameBgHovered]       = bg3;
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.45f, 0.45f, 0.45f, 1.0f);
    c[ImGuiCol_TitleBg]              = bg1;
    c[ImGuiCol_TitleBgActive]        = bg1;
    c[ImGuiCol_TitleBgCollapsed]     = bg1;
    c[ImGuiCol_MenuBarBg]            = bg1;
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_ScrollbarGrab]        = bg3;
    c[ImGuiCol_ScrollbarGrabHovered] = bg4;
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.40f, 0.40f, 0.40f, 1.0f);
    c[ImGuiCol_CheckMark]            = ImVec4(1.0f, 1.0f, 1.0f, 0.90f);
    c[ImGuiCol_CheckboxSelectedBg]   = bg2;
    c[ImGuiCol_SliderGrab]           = acc;
    c[ImGuiCol_SliderGrabActive]     = accA;
    c[ImGuiCol_Button]               = bg2;
    c[ImGuiCol_ButtonHovered]        = bg3;
    c[ImGuiCol_ButtonActive]         = bg4;
    c[ImGuiCol_Header]               = accDim;
    c[ImGuiCol_HeaderHovered]        = ImVec4(acc.x, acc.y, acc.z, 0.35f);
    c[ImGuiCol_HeaderActive]         = ImVec4(acc.x, acc.y, acc.z, 0.50f);
    c[ImGuiCol_Separator]            = ImVec4(1.0f, 1.0f, 1.0f, 0.12f);
    c[ImGuiCol_SeparatorHovered]     = accH;
    c[ImGuiCol_SeparatorActive]      = acc;
    c[ImGuiCol_ResizeGrip]           = ImVec4(acc.x, acc.y, acc.z, 0.20f);
    c[ImGuiCol_ResizeGripHovered]    = ImVec4(acc.x, acc.y, acc.z, 0.50f);
    c[ImGuiCol_ResizeGripActive]     = ImVec4(acc.x, acc.y, acc.z, 0.80f);
    c[ImGuiCol_Tab]                  = bg1;
    c[ImGuiCol_TabHovered]           = bg3;
    c[ImGuiCol_TabSelected]          = bg0;
    c[ImGuiCol_TabDimmed]            = bg1;
    c[ImGuiCol_TabDimmedSelected]    = bg2;
    c[ImGuiCol_DockingPreview]       = ImVec4(acc.x, acc.y, acc.z, 0.40f);
    c[ImGuiCol_DockingEmptyBg]       = bg1;
    c[ImGuiCol_PlotLines]            = acc;
    c[ImGuiCol_PlotLinesHovered]     = accH;
    c[ImGuiCol_PlotHistogram]        = acc;
    c[ImGuiCol_PlotHistogramHovered] = accH;
    c[ImGuiCol_TableHeaderBg]        = bg2;
    c[ImGuiCol_TableBorderStrong]    = brd;
    c[ImGuiCol_TableBorderLight]     = ImVec4(1.0f, 1.0f, 1.0f, 0.05f);
    c[ImGuiCol_NavHighlight]         = acc;

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

    if (glfwWindowShouldClose(m_window) && m_sceneDirty) {
        glfwSetWindowShouldClose(m_window, 0);
        m_showUnsavedChangesDialog = true;
    }

    RenderDockspace();
    RenderHierarchy();
    RenderViewport();
    RenderInspector();
    RenderConsole();
    RenderAssetBrowser();

    if (m_nodeEditor) m_nodeEditor->Render();
    RenderUnsavedChangesDialog();
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
    dl->AddCircleFilled(ImVec2(wPos.x + 20.0f, wPos.y + kTitleH * 0.5f), 4.5f,
        IM_COL32(192, 54, 76, 255));
    dl->AddText(ImVec2(wPos.x + 32.0f, textY),
        IM_COL32(210, 210, 220, 255), "Engine");

    if (!m_currentScenePath.empty() || m_sceneDirty) {
        std::string label = m_currentScenePath.empty()
            ? "Untitled"
            : std::filesystem::path(m_currentScenePath).filename().string();
        if (m_sceneDirty) label = "\xe2\x80\xa2 " + label;
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
        if (RMenuItem("New Scene",        "Ctrl+N",        ImDrawFlags_RoundCornersTop)) {
            m_gameObjects.clear(); m_objectCounter = 0;
            m_selectedObjectIndex = -1; m_currentScenePath.clear(); m_sceneDirty = false;
            AddGameObject("Main Camera", PrimitiveType::Camera);
            LogSuccess("New scene created");
        }
        if (MItem("Open Scene", "Ctrl+O")) {
            std::string path = OpenSceneFileDialog();
            if (!path.empty()) {
                std::vector<GameObject> loaded;
                if (SceneSerializer::Load(path, loaded)) {
                    m_gameObjects = std::move(loaded);
                    m_selectedObjectIndex = -1;
                    m_currentScenePath = path; m_sceneDirty = false;
                    LogSuccess("Scene loaded: " + path);
                } else { LogError("Failed to load scene: " + path); }
            }
        }
        ImGui::Separator();
        if (MItem("Save Scene",       "Ctrl+S"))       SaveCurrentScene();
        if (MItem("Save Scene As...", "Ctrl+Shift+S")) {
            std::string prev = m_currentScenePath; m_currentScenePath.clear();
            if (!SaveCurrentScene()) m_currentScenePath = prev;
        }
        ImGui::Separator();
        if (RMenuItem("Exit",    "Alt+F4", ImDrawFlags_RoundCornersBottom))
            RequestClose();
        PopupCloseCheck();
        ImGui::EndPopup();
    }

    MenuBtn("Edit", "##mEdit");
    ImGui::SetNextWindowPos(popupPos, ImGuiCond_Always);
    if (ImGui::BeginPopup("##mEdit", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar)) {
        if (RMenuItem("Undo", "Ctrl+Z", ImDrawFlags_RoundCornersTop))    LogInfo("Undo (not implemented)");
        if (MItem("Redo",     "Ctrl+Y"))                                  LogInfo("Redo (not implemented)");
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
            if (!src.empty()) {
                std::string emdl = ModelImporter::Import(src);
                if (!emdl.empty()) AddModelObject(emdl);
                else LogError("Failed to import model: " + src);
            }
        }
        PopupCloseCheck();
        ImGui::EndPopup();
    }

    MenuBtn("Help", "##mHelp", false);
    ImGui::SetNextWindowPos(popupPos, ImGuiCond_Always);
    if (ImGui::BeginPopup("##mHelp", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar)) {
        if (RMenuItem("About", nullptr, ImDrawFlags_RoundCornersAll))
            LogInfo("Engine v0.1 - Built with C++ and ImGui");
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
            ".emat", ".emdl", ".escn",
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
        } else if (item.ext == ".escn") {
            typeColor = {0.62f, 0.32f, 0.92f, 1.0f}; typeLabel = "SCN";
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
                std::string emdl = ModelImporter::Import(item.path);
                if (!emdl.empty()) AddModelObject(emdl);
                else LogError("Import failed: " + item.name);
                m_assetBrowserDirty = true;
            } else if (item.ext == ".escn") {
                std::vector<GameObject> loaded;
                if (SceneSerializer::Load(item.path, loaded)) {
                    m_gameObjects = std::move(loaded);
                    m_selectedObjectIndex = -1;
                    m_currentScenePath = item.path;
                    m_sceneDirty = false;
                    LogSuccess("Scene loaded: " + item.name);
                } else {
                    LogError("Failed to load scene: " + item.name);
                }
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

    ImGui::TextDisabled("%zu objects in scene", m_gameObjects.size());
    ImGui::Separator();

    for (int i = 0; i < (int)m_gameObjects.size(); i++) {
        RenderGameObjectNode(m_gameObjects[i], i);
    }

    ImGui::Separator();

    if (ImGui::Button("Add Object")) {
        std::string sourcePath = OpenModelFileDialog();
        if (!sourcePath.empty()) {
            std::string emdlPath = ModelImporter::Import(sourcePath);
            if (!emdlPath.empty())
                AddModelObject(emdlPath);
            else
                LogError("Failed to import model: " + sourcePath);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete Selected")) {
        if (m_selectedObjectIndex >= 0 && m_selectedObjectIndex < (int)m_gameObjects.size()) {
            std::string name = m_gameObjects[m_selectedObjectIndex].name;
            m_gameObjects.erase(m_gameObjects.begin() + m_selectedObjectIndex);
            m_selectedObjectIndex = -1;
            m_sceneDirty = true;
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

    if (!obj.materialPath.empty() && m_previewRenderer) {
        Material* hmat = MaterialManager::GetOrLoad(obj.materialPath);
        if (hmat) {
            uint32_t prev = m_previewRenderer->GetPreview(hmat, obj.materialPath);
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

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ASSET_EMAT")) {
            obj.materialPath = std::string(static_cast<const char*>(p->Data));
            if (m_nodeEditor) m_nodeEditor->EnsureCompiled(obj.materialPath, m_sceneRenderer);
            m_sceneDirty = true;
            LogInfo("Material assigned to " + obj.name);
        }
        ImGui::EndDragDropTarget();
    }

    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Duplicate")) {
            GameObject newObj = obj;
            newObj.name = obj.name + "_copy";
            m_gameObjects.push_back(newObj);
            m_sceneDirty = true;
            LogInfo("Duplicated: " + obj.name);
        }
        if (ImGui::MenuItem("Delete")) {
            m_gameObjects.erase(m_gameObjects.begin() + index);
            if (m_selectedObjectIndex == index)
                m_selectedObjectIndex = -1;
            m_sceneDirty = true;
            LogInfo("Deleted: " + obj.name);
        }
        ImGui::EndPopup();
    }
}

void EditorUI::RenderViewportToolbar() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(30, 30, 35, 255));
    ImGui::BeginChild("##vptoolbar", ImVec2(0, kViewportToolbarH), false,
                      ImGuiWindowFlags_NoScrollbar);

    const ImVec2 bsz(30.0f, 26.0f);
    ImGui::SetCursorPos(ImVec2(8.0f, (kViewportToolbarH - bsz.y) * 0.5f));

    // Play / Stop — placeholders until play mode exists.
    if (IconButton(ICON_FA_PLAY, "##vp_play", bsz)) { /* TODO: enter play mode */ }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Play (coming soon)");
    ImGui::SameLine(0, 4);
    if (IconButton(ICON_FA_STOP, "##vp_stop", bsz)) { /* TODO: stop */ }
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

void EditorUI::RenderViewport() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    // Icon toolbar above the 3D view (play / render / settings).
    RenderViewportToolbar();

    // The 3D view fills the space between the toolbar and the bottom timeline.
    ImVec2 viewportSize = ImGui::GetContentRegionAvail();
    viewportSize.y = (viewportSize.y > kViewportTimelineH + 1.0f)
                     ? viewportSize.y - kViewportTimelineH : 1.0f;
    m_viewportWidth  = viewportSize.x;
    m_viewportHeight = viewportSize.y;

    float aspect = (viewportSize.y > 0) ? viewportSize.x / viewportSize.y : 1.0f;
    glm::mat4 view       = m_camera.GetViewMatrix();
    glm::mat4 projection = m_camera.GetProjectionMatrix(aspect);

    ImVec2 imageScreenPos = ImGui::GetCursorScreenPos();

    if (m_sceneRenderer && viewportSize.x > 0 && viewportSize.y > 0) {
        uint32_t textureID = m_sceneRenderer->Render(
            m_gameObjects, m_selectedObjectIndex,
            (int)viewportSize.x, (int)viewportSize.y,
            view, projection);

        if (textureID != 0)
            ImGui::Image((ImTextureID)(intptr_t)textureID, viewportSize, ImVec2(0, 1), ImVec2(1, 0));

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ASSET_EMAT")) {
                std::string matPath(static_cast<const char*>(p->Data));

                ImVec2 mousePos = ImGui::GetIO().MousePos;
                ImVec2 vpMin    = imageScreenPos;
                float ndcX = (2.0f * (mousePos.x - vpMin.x)) / viewportSize.x - 1.0f;
                float ndcY = 1.0f - (2.0f * (mousePos.y - vpMin.y)) / viewportSize.y;
                int hitIdx = PickObject(m_gameObjects, view, projection, ndcX, ndcY);

                int targetIdx = (hitIdx >= 0) ? hitIdx : m_selectedObjectIndex;
                if (targetIdx >= 0 && targetIdx < (int)m_gameObjects.size()) {
                    m_gameObjects[targetIdx].materialPath = matPath;
                    if (m_nodeEditor) m_nodeEditor->EnsureCompiled(matPath, m_sceneRenderer);
                    m_selectedObjectIndex = targetIdx;
                    m_sceneDirty = true;
                    namespace fs = std::filesystem;
                    LogInfo("Material \"" + fs::path(matPath).stem().string()
                        + "\" в†’ " + m_gameObjects[targetIdx].name);
                }
            }

            if (ImGui::GetDragDropPayload() &&
                ImGui::GetDragDropPayload()->IsDataType("ASSET_EMAT"))
            {
                ImVec2 mousePos = ImGui::GetIO().MousePos;
                ImVec2 vpMin    = imageScreenPos;
                float ndcX = (2.0f * (mousePos.x - vpMin.x)) / viewportSize.x - 1.0f;
                float ndcY = 1.0f - (2.0f * (mousePos.y - vpMin.y)) / viewportSize.y;
                int hitIdx = PickObject(m_gameObjects, view, projection, ndcX, ndcY);

                int targetIdx = (hitIdx >= 0) ? hitIdx : m_selectedObjectIndex;
                ImGui::BeginTooltip();
                if (targetIdx >= 0 && targetIdx < (int)m_gameObjects.size())
                    ImGui::Text("Assign to: %s", m_gameObjects[targetIdx].name.c_str());
                else
                    ImGui::TextDisabled("No object under cursor");
                ImGui::EndTooltip();
            }

            ImGui::EndDragDropTarget();
        }
    }

    // ── Light gizmo icons + selected-light range overlay ─────────────────
    // Lights have no mesh; draw a billboarded bulb icon so they're visible.
    {
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

        for (size_t i = 0; i < m_gameObjects.size(); ++i) {
            const GameObject& o = m_gameObjects[i];
            if (o.type != PrimitiveType::Light) continue;

            glm::vec3 pos(o.position[0], o.position[1], o.position[2]);
            bool sel = (static_cast<int>(i) == m_selectedObjectIndex);

            ImVec2 sp;
            bool   onScreen = toScreen(pos, sp);

            // Range / cone overlay — only for the selected light, kept subtle.
            if (sel) {
                glm::mat4 m   = BuildModelMatrix(o);
                glm::vec3 dir = glm::normalize(glm::mat3(m) * glm::vec3(0.0f, 0.0f, -1.0f));
                ImU32 line = IM_COL32((int)(o.lightColor[0] * 255), (int)(o.lightColor[1] * 255),
                                      (int)(o.lightColor[2] * 255), 70);   // low alpha = subtle

                if (o.lightType == 1) {                       // Point → wireframe sphere
                    worldCircle(pos, glm::vec3(1,0,0), glm::vec3(0,1,0), o.lightRange, line);
                    worldCircle(pos, glm::vec3(0,1,0), glm::vec3(0,0,1), o.lightRange, line);
                    worldCircle(pos, glm::vec3(1,0,0), glm::vec3(0,0,1), o.lightRange, line);
                } else if (o.lightType == 2) {                // Spot → cone
                    glm::vec3 u, v; planeBasis(dir, u, v);
                    glm::vec3 base   = pos + dir * o.lightRange;
                    float     radius = o.lightRange * tanf(glm::radians(o.spotOuterDeg));
                    worldCircle(base, u, v, radius, line);
                    for (int k = 0; k < 4; ++k) {             // 4 edge lines apex → rim
                        float t = (float)k / 4 * 2.0f * kPi;
                        glm::vec3 rim = base + radius * (cosf(t) * u + sinf(t) * v);
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

            ImU32  tint = IM_COL32((int)(o.lightColor[0] * 255), (int)(o.lightColor[1] * 255),
                                   (int)(o.lightColor[2] * 255), 255);
            const char* icon = (o.lightType == 0) ? ICON_FA_SUN : ICON_FA_LIGHTBULB;
            ImVec2 ts = ImGui::CalcTextSize(icon);
            ImVec2 tp(sp.x - ts.x * 0.5f, sp.y - ts.y * 0.5f);

            if (sel) dl->AddCircle(sp, ts.x * 0.9f, IM_COL32(255, 200, 80, 220), 0, 2.0f);
            dl->AddText(ImVec2(tp.x + 1, tp.y + 1), IM_COL32(0, 0, 0, 160), icon);  // shadow
            dl->AddText(tp, tint, icon);
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

    if (m_selectedObjectIndex >= 0 && m_selectedObjectIndex < (int)m_gameObjects.size()) {
        GameObject& sel = m_gameObjects[m_selectedObjectIndex];

        if (sel.type != PrimitiveType::Empty) {
            glm::mat4 model = BuildModelMatrix(sel);
            glm::mat4 delta(1.0f);

            if (ImGuizmo::Manipulate(
                    glm::value_ptr(view), glm::value_ptr(projection),
                    m_gizmoOp, m_gizmoMode,
                    glm::value_ptr(model),
                    glm::value_ptr(delta)))
            {
                if (m_gizmoOp == ImGuizmo::TRANSLATE) {
                    // delta[3] is the displacement in world space for both
                    // WORLD and LOCAL modes вЂ” avoids the mModelSource*T_delta
                    // multiplication-order bug that rotates the delta.
                    sel.position[0] += delta[3][0];
                    sel.position[1] += delta[3][1];
                    sel.position[2] += delta[3][2];
                } else {
                    // ROTATE / SCALE: extract directly from the result matrix.
                    sel.position[0] = model[3][0];
                    sel.position[1] = model[3][1];
                    sel.position[2] = model[3][2];

                    sel.scale[0] = glm::length(glm::vec3(model[0]));
                    sel.scale[1] = glm::length(glm::vec3(model[1]));
                    sel.scale[2] = glm::length(glm::vec3(model[2]));

                    if (sel.scale[0] > 1e-5f && sel.scale[1] > 1e-5f && sel.scale[2] > 1e-5f) {
                        glm::mat3 rotMat(
                            glm::vec3(model[0]) / sel.scale[0],
                            glm::vec3(model[1]) / sel.scale[1],
                            glm::vec3(model[2]) / sel.scale[2]);
                        glm::quat q = glm::quat_cast(rotMat);
                        sel.rotQuat[0] = q.x; sel.rotQuat[1] = q.y;
                        sel.rotQuat[2] = q.z; sel.rotQuat[3] = q.w;
                    }
                }
                m_sceneDirty = true;
            }
        }
    }

    ImVec2 imagePos = ImVec2(imageScreenPos.x - ImGui::GetWindowPos().x,
                             imageScreenPos.y - ImGui::GetWindowPos().y);

    if (viewportSize.x > 0 && viewportSize.y > 0 && !ImGuizmo::IsUsing() && !ImGuizmo::IsOver()) {
        ImGui::SetCursorPos(imagePos);
        ImGui::InvisibleButton("ViewportInteraction", viewportSize,
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);

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

        if (ImGui::IsItemDeactivated() && m_viewportDragAccum < 4.0f && !ImGuizmo::IsOver()) {
            ImVec2 itemMin  = ImGui::GetItemRectMin();
            ImVec2 mousePos = ImGui::GetIO().MousePos;
            float ndcX = (2.0f * (mousePos.x - itemMin.x)) / viewportSize.x - 1.0f;
            float ndcY = 1.0f - (2.0f * (mousePos.y - itemMin.y)) / viewportSize.y;

            int hitIndex = PickObject(m_gameObjects, view, projection, ndcX, ndcY);
            if (hitIndex >= 0) {
                SelectGameObject(hitIndex);
                LogInfo("Selected: " + m_gameObjects[hitIndex].name);
            } else {
                m_selectedObjectIndex = -1;
            }
        }
    }

    if (ImGui::IsWindowFocused() && !ImGuizmo::IsUsing()) {
        if (ImGui::IsKeyPressed(ImGuiKey_M)) m_gizmoOp = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) m_gizmoOp = ImGuizmo::ROTATE;
        if (ImGui::IsKeyPressed(ImGuiKey_S)) m_gizmoOp = ImGuizmo::SCALE;
        if (ImGui::IsKeyPressed(ImGuiKey_X))
            m_gizmoMode = (m_gizmoMode == ImGuizmo::WORLD) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
    }

    ImGui::SetCursorPos(ImVec2(imagePos.x + 8.0f, imagePos.y + 8.0f));
    {
        const char* opName = (m_gizmoOp == ImGuizmo::TRANSLATE) ? "Move (M)"  :
                             (m_gizmoOp == ImGuizmo::ROTATE)    ? "Rotate (R)":
                                                                   "Scale (S)";
        const char* modeName = (m_gizmoMode == ImGuizmo::WORLD) ? "World" : "Local";
        ImGui::TextColored(ImVec4(0.72f, 0.72f, 0.75f, 1.0f),
            "%zu objects  |  %s  |  %s (X)  |  %.0f FPS",
            m_gameObjects.size(), opName, modeName, ImGui::GetIO().Framerate);
    }

    // Timeline strip below the 3D view (placeholder for now).
    RenderViewportTimeline();

    ImGui::End();
}

void EditorUI::RenderInspector() {
    if (!m_showInspector) return;

    ImGui::Begin("Inspector", &m_showInspector, ImGuiWindowFlags_NoCollapse);

    GameObject* selected = GetSelectedObject();
    if (selected) {
        ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.88f, 1.0f), "Selected: %s", selected->name.c_str());
        ImGui::TextDisabled("Type: %s", ToString(selected->type));
        ImGui::Separator();

        ImGui::Text("Transform");
        ImGui::Indent();
        RenderTransformEditor(selected);
        ImGui::Unindent();

        if (selected->type == PrimitiveType::Light) {
            ImGui::Separator();
            ImGui::Text("Light");
            ImGui::Indent();
            bool lc = false;

            const char* kinds[] = { "Directional", "Point", "Spot" };
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##ltype", &selected->lightType, kinds, 3)) lc = true;
            if (selected->lightType < 0) selected->lightType = 0;
            if (selected->lightType > 2) selected->lightType = 2;

            ImGui::TextDisabled("Color");
            if (ImGui::ColorEdit3("##lcol", selected->lightColor,
                                  ImGuiColorEditFlags_NoInputs)) lc = true;

            ImGui::TextDisabled("Intensity");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat("##lint", &selected->lightIntensity, 0.05f, 0.0f, 100.0f, "%.2f")) lc = true;

            if (selected->lightType != 0) {  // point / spot
                ImGui::TextDisabled("Range");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##lrange", &selected->lightRange, 0.1f, 0.1f, 1000.0f, "%.1f")) lc = true;
            }
            if (selected->lightType == 2) {  // spot
                ImGui::TextDisabled("Cone (inner / outer deg)");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat2("##lcone", &selected->spotInnerDeg, 0.2f, 0.0f, 89.0f, "%.1f")) lc = true;
                if (selected->spotInnerDeg > selected->spotOuterDeg)
                    selected->spotInnerDeg = selected->spotOuterDeg;
            }

            if (lc) m_sceneDirty = true;
            ImGui::Unindent();
        }

        if (selected->type == PrimitiveType::Model) {
            ImGui::Separator();
            ImGui::Text("Model Asset");
            ImGui::Indent();
            ImGui::TextDisabled("%s", selected->modelPath.c_str());
            if (ImGui::Button("Reimport")) {
                std::filesystem::path emdl(selected->modelPath);
                std::string guessedSource = emdl.replace_extension("").string();
                for (const char* ext : { ".obj", ".fbx", ".gltf", ".glb" }) {
                    std::string candidate = guessedSource + ext;
                    if (std::filesystem::exists(candidate)) {
                        std::string newEmdl = ModelImporter::Import(candidate);
                        if (!newEmdl.empty()) {
                            glm::vec3 mn, mx;
                            if (ModelMesh::ReadAabb(newEmdl, mn, mx)) {
                                selected->aabbMin[0] = mn.x; selected->aabbMin[1] = mn.y; selected->aabbMin[2] = mn.z;
                                selected->aabbMax[0] = mx.x; selected->aabbMax[1] = mx.y; selected->aabbMax[2] = mx.z;
                            }
                            LogSuccess("Reimported: " + candidate);
                        }
                        break;
                    }
                }
            }
            ImGui::Unindent();
        }

        ImGui::Separator();
        ImGui::Text("Material");
        ImGui::Indent();
        if (selected->materialPath.empty()) {
            ImGui::TextDisabled("No material assigned");
            ImGui::TextDisabled("Drag .emat from Asset Browser");
        } else {
            namespace fs = std::filesystem;
            if (m_previewRenderer) {
                Material* pm = MaterialManager::GetOrLoad(selected->materialPath);
                if (pm) {
                    uint32_t pt = m_previewRenderer->GetPreview(pm, selected->materialPath);
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
            ImGui::TextDisabled("%s", fs::path(selected->materialPath).filename().string().c_str());
            ImGui::SameLine(ImGui::GetContentRegionMax().x - 20.0f);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.0f);
            if (IconButton(ICON_FA_XMARK, "##detachmat", ImVec2(20.0f, 20.0f))) {
                selected->materialPath.clear();
                m_sceneDirty = true;
            }
            Material* mat = MaterialManager::GetOrLoad(selected->materialPath);
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

                if (changed) { mat->Save(selected->materialPath); m_sceneDirty = true; }

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
                            mat->Save(selected->materialPath);
                            m_sceneDirty = true;
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Remove##tex")) {
                        TextureManager::Invalidate(mat->albedoTexture);
                        mat->albedoTexture.clear();
                        mat->Save(selected->materialPath);
                        m_sceneDirty = true;
                    }
                    ImGui::EndGroup();
                } else {
                    ImGui::TextDisabled("None");
                    if (ImGui::Button("Load Texture...")) {
                        std::string p = OpenTextureFileDialog();
                        if (!p.empty()) {
                            mat->albedoTexture = p;
                            mat->Save(selected->materialPath);
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
                            if (!p.empty()) { TextureManager::Invalidate(mat->normalTexture); mat->normalTexture = p; mat->Save(selected->materialPath); m_sceneDirty = true; }
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Remove##nrm")) { TextureManager::Invalidate(mat->normalTexture); mat->normalTexture.clear(); mat->Save(selected->materialPath); m_sceneDirty = true; }
                        ImGui::EndGroup();
                    } else {
                        if (ImGui::Button("Load Normal Map...")) {
                            std::string p = OpenTextureFileDialog();
                            if (!p.empty()) { mat->normalTexture = p; mat->Save(selected->materialPath); m_sceneDirty = true; }
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
                                mat->Save(selected->materialPath);
                                m_sceneDirty = true;
                            }
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Remove##mr")) {
                            TextureManager::Invalidate(mat->ormTexture);
                            mat->ormTexture.clear();
                            mat->Save(selected->materialPath);
                            m_sceneDirty = true;
                        }
                        ImGui::EndGroup();
                    } else {
                        if (ImGui::Button("Load MR Texture...")) {
                            std::string p = OpenTextureFileDialog();
                            if (!p.empty()) {
                                mat->ormTexture = p;
                                mat->Save(selected->materialPath);
                                m_sceneDirty = true;
                            }
                        }
                    }
                }

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    if (m_nodeEditor)
                        m_nodeEditor->Open(selected->materialPath, m_previewRenderer, m_sceneRenderer);
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Double-click to open Node Editor");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "File not found");
                if (ImGui::Button("Clear")) {
                    selected->materialPath.clear();
                    m_sceneDirty = true;
                }
            }
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

void EditorUI::RenderTransformEditor(GameObject* obj) {
    if (!obj) return;

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

    if (XYZRow("Position", obj->position, 0.1f, -1e6f, 1e6f, "%.2f"))
        m_sceneDirty = true;

    ImGui::Spacing();

    glm::quat q(obj->rotQuat[3], obj->rotQuat[0], obj->rotQuat[1], obj->rotQuat[2]);
    glm::vec3 euler = glm::degrees(glm::eulerAngles(q));
    float eulerArr[3] = { euler.x, euler.y, euler.z };
    if (XYZRow("Rotation", eulerArr, 0.5f, -360.0f, 360.0f, "%.1f")) {
        glm::quat nq = glm::quat(glm::radians(glm::vec3(eulerArr[0], eulerArr[1], eulerArr[2])));
        obj->rotQuat[0] = nq.x; obj->rotQuat[1] = nq.y;
        obj->rotQuat[2] = nq.z; obj->rotQuat[3] = nq.w;
        m_sceneDirty = true;
    }

    ImGui::Spacing();

    if (XYZRow("Scale", obj->scale, 0.05f, 0.001f, 1e4f, "%.3f"))
        m_sceneDirty = true;
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

bool EditorUI::SaveCurrentScene() {
    std::string path = m_currentScenePath;
    if (path.empty()) {
        path = SaveSceneFileDialog();
        if (path.empty()) return false;
    }
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    if (!SceneSerializer::Save(path, m_gameObjects)) {
        LogError("Failed to save scene: " + path);
        return false;
    }
    AssetDatabase::GetOrCreateGuid(path, "Scene");
    m_currentScenePath = path;
    m_sceneDirty = false;
    LogSuccess("Scene saved: " + path);
    return true;
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
            if (SaveCurrentScene())
                glfwSetWindowShouldClose(m_window, 1);
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

void EditorUI::AddGameObject(const std::string& name, PrimitiveType type) {
    GameObject obj;
    obj.name = name + "_" + std::to_string(m_objectCounter++);
    obj.type = type;
    m_gameObjects.push_back(obj);
    m_sceneDirty = true;
    LogInfo("Created: " + obj.name);
}

void EditorUI::AddModelObject(const std::string& emdlPath) {
    GameObject obj;
    obj.type      = PrimitiveType::Model;
    obj.modelPath = emdlPath;
    obj.name      = std::filesystem::path(emdlPath).stem().string()
                    + "_" + std::to_string(m_objectCounter++);

    glm::vec3 mn, mx;
    if (ModelMesh::ReadAabb(emdlPath, mn, mx)) {
        obj.aabbMin[0] = mn.x; obj.aabbMin[1] = mn.y; obj.aabbMin[2] = mn.z;
        obj.aabbMax[0] = mx.x; obj.aabbMax[1] = mx.y; obj.aabbMax[2] = mx.z;
    }

    m_gameObjects.push_back(obj);
    m_sceneDirty = true;
    LogSuccess("Model added: " + obj.name);
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
