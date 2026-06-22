#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "EditorUI.h"
#include "IconsFontAwesome6.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include "glfw3native.h"
#include <windows.h>
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
#include <windows.h>
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
            ImGui::SetTooltip("%s  —  click to edit", hex);
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
    RenderMaterialEditor();
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
            if      (fromRight <= kBtnW)       glfwSetWindowShouldClose(m_window, 1);
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
    ImGuiID dockspaceID = ImGui::GetID("EditorDockspaceV2");
    ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    static bool layoutBuilt = false;
    if (!layoutBuilt) {
        layoutBuilt = true;

        ImVec2 dockSize = ImVec2(viewport->Size.x, viewport->Size.y - kHeaderH);
        ImGui::DockBuilderRemoveNode(dockspaceID);
        ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceID, dockSize);

        ImGuiID dockMain   = dockspaceID;
        ImGuiID dockBottom = ImGui::DockBuilderSplitNode(dockMain,   ImGuiDir_Down,  0.26f, nullptr, &dockMain);
        ImGuiID dockLeft   = ImGui::DockBuilderSplitNode(dockMain,   ImGuiDir_Left,  0.18f, nullptr, &dockMain);
        ImGuiID dockRight  = ImGui::DockBuilderSplitNode(dockMain,   ImGuiDir_Right, 0.22f, nullptr, &dockMain);

        ImGuiID dockBotRight = 0;
        ImGuiID dockBotLeft  = ImGui::DockBuilderSplitNode(dockBottom, ImGuiDir_Left, 0.58f,
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
            glfwSetWindowShouldClose(m_window, true);
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

void EditorUI::RenderMaterialEditor() {
    if (m_showMaterialEditor && !ImGui::IsPopupOpen("##MatEditor"))
        ImGui::OpenPopup("##MatEditor");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(740, 530), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,  12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(22.0f, 18.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

    constexpr ImGuiWindowFlags kModalFlags =
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar;

    if (ImGui::BeginPopupModal("##MatEditor", nullptr, kModalFlags)) {
        namespace fs = std::filesystem;

        Material* mat = m_editingMaterialPath.empty()
            ? nullptr : MaterialManager::GetOrLoad(m_editingMaterialPath);

        // ── Header ────────────────────────────────────────────────────────
        {
            constexpr float kCloseW = 22.0f;
            ImGui::Text("Material Editor");
            ImGui::SameLine(ImGui::GetContentRegionMax().x - kCloseW);
            if (IconButton(ICON_FA_XMARK, "##closematEd", ImVec2(kCloseW, kCloseW))) {
                ImGui::CloseCurrentPopup();
                m_showMaterialEditor = false;
            }
        }
        ImGui::Separator();
        ImGui::Spacing();

        if (!mat) {
            ImGui::TextDisabled("Material not found.");
            ImGui::EndPopup();
            ImGui::PopStyleVar(3);
            return;
        }

        // ── Material Name ─────────────────────────────────────────────────
        static char s_nameBuf[256] = "";
        static std::string s_lastPath;
        if (s_lastPath != m_editingMaterialPath) {
            s_lastPath = m_editingMaterialPath;
            snprintf(s_nameBuf, sizeof(s_nameBuf), "%s",
                fs::path(m_editingMaterialPath).stem().string().c_str());
        }

        ImGui::Text("Name");
        ImGui::SameLine(70.0f);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::InputText("##matname", s_nameBuf, sizeof(s_nameBuf));

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ── Preview + Properties ──────────────────────────────────────────
        bool changed = false;
        constexpr float kPreviewSz = 188.0f;
        constexpr float kLabelW    = 88.0f;

        // Sphere preview
        ImGui::BeginGroup();
        {
            uint32_t prevTex = m_previewRenderer ? m_previewRenderer->Render(mat) : 0;
            if (prevTex)
                ImGui::Image((ImTextureID)(intptr_t)prevTex, ImVec2(kPreviewSz, kPreviewSz));
            else {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.12f, 1.0f));
                ImGui::BeginChild("##noprev", ImVec2(kPreviewSz, kPreviewSz), false);
                ImGui::EndChild();
                ImGui::PopStyleColor();
            }
        }
        ImGui::EndGroup();

        ImGui::SameLine(0, 18);

        // Properties column
        ImGui::BeginGroup();
        {
            auto Row = [&](const char* lbl, auto fn) {
                ImGui::BeginTable("##pr", 2, ImGuiTableFlags_SizingFixedFit);
                ImGui::TableSetupColumn("l", ImGuiTableColumnFlags_WidthFixed, kLabelW);
                ImGui::TableSetupColumn("c", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                ImGui::TextDisabled("%s", lbl);
                ImGui::TableSetColumnIndex(1);
                if (fn()) changed = true;
                ImGui::EndTable();
            };

            constexpr float kIconBtn = 22.0f;
            auto TexRow = [&](const char* lbl, std::string& path) {
                ImGui::BeginTable(("##tx" + std::string(lbl)).c_str(), 4,
                    ImGuiTableFlags_SizingFixedFit);
                ImGui::TableSetupColumn("l", ImGuiTableColumnFlags_WidthFixed,   kLabelW);
                ImGui::TableSetupColumn("n", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("b", ImGuiTableColumnFlags_WidthFixed,   kIconBtn);
                ImGui::TableSetupColumn("x", ImGuiTableColumnFlags_WidthFixed,   kIconBtn);
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.0f);
                ImGui::TextDisabled("%s", lbl);

                ImGui::TableSetColumnIndex(1);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.0f);
                if (path.empty()) ImGui::TextDisabled("None");
                else ImGui::TextDisabled("%s", fs::path(path).filename().string().c_str());

                ImGui::TableSetColumnIndex(2);
                {
                    char bid[32]; snprintf(bid, sizeof(bid), "##ld%s", lbl);
                    if (IconButton(ICON_FA_FOLDER_OPEN, bid, ImVec2(kIconBtn, kIconBtn)))
                    {
                        std::string p = OpenTextureFileDialog();
                        if (!p.empty()) { path = p; changed = true; }
                    }
                }

                ImGui::TableSetColumnIndex(3);
                if (!path.empty()) {
                    char cid[32]; snprintf(cid, sizeof(cid), "##cx%s", lbl);
                    ImU32 redCol = IM_COL32(178, 76, 76, 255);
                    if (IconButton(ICON_FA_XMARK, cid, ImVec2(kIconBtn, kIconBtn), redCol))
                        { path.clear(); changed = true; }
                }
                ImGui::EndTable();
            };

            ImGui::TextDisabled("Surface");
            Row("Base Color", [&]{ return ColorSwatchEdit("##mbcol",mat->color,true); });
            Row("Metallic",   [&]{ return FillSlider("##mmet","Metallic", &mat->metallic,  0.0f, 1.0f); });
            Row("Roughness",  [&]{ return FillSlider("##mrou","Roughness",&mat->roughness, 0.0f, 1.0f); });

            ImGui::Spacing();
            ImGui::TextDisabled("Emissive");
            Row("Color",     [&]{ return ColorSwatchEdit("##mecol",mat->emissiveColor,false); });
            Row("Intensity", [&]{ return FillSlider("##mei","Intensity",&mat->emissiveIntensity,0.0f,10.0f,"%.1f"); });

            ImGui::Spacing();
            ImGui::TextDisabled("Textures");
            TexRow("Albedo", mat->albedoTexture);
            TexRow("Normal", mat->normalTexture);
            TexRow("ORM",    mat->ormTexture);
        }
        ImGui::EndGroup();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float btnW = 90.0f;
        ImGui::SetCursorPosX((ImGui::GetContentRegionMax().x - btnW) * 0.5f);
        if (ImGui::Button("Save##matsave", ImVec2(btnW, 0))) {
            std::string trimmed = s_nameBuf;
            auto f = trimmed.find_first_not_of(" \t");
            if (f == std::string::npos) trimmed.clear();
            else {
                trimmed = trimmed.substr(f);
                auto l = trimmed.find_last_not_of(" \t");
                if (l != std::string::npos) trimmed = trimmed.substr(0, l + 1);
            }

            std::string currentStem = fs::path(m_editingMaterialPath).stem().string();
            if (!trimmed.empty() && trimmed != currentStem) {
                std::string     newFile = trimmed + ".emat";
                fs::path        dir     = fs::path(m_editingMaterialPath).parent_path();
                std::string     newPath = (dir / newFile).string();
                std::error_code ec;
                bool            exists  = fs::exists(newPath, ec);
                if (!ec && !exists) {
                    MaterialManager::Invalidate(m_editingMaterialPath);
                    fs::rename(m_editingMaterialPath, newPath, ec);
                    if (!ec) {
                        for (auto& obj : m_gameObjects)
                            if (obj.materialPath == m_editingMaterialPath)
                                obj.materialPath = newPath;
                        m_editingMaterialPath = newPath;
                        m_assetBrowserDirty   = true;
                        s_lastPath            = "";
                    } else {
                        snprintf(s_nameBuf, sizeof(s_nameBuf), "%s", currentStem.c_str());
                    }
                }
            }

            if (m_previewRenderer)
                m_previewRenderer->InvalidatePreview(m_editingMaterialPath);
            mat->Save(m_editingMaterialPath);
            m_sceneDirty = true;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(3);

    if (!ImGui::IsPopupOpen("##MatEditor"))
        m_showMaterialEditor = false;
}

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

    constexpr float kCell = 72.0f;
    constexpr float kPad  =  6.0f;
    constexpr float kNameH = 18.0f;
    const float cellTotal = kCell + kPad * 2.0f;

    float panelW  = ImGui::GetContentRegionAvail().x;
    int   columns = std::max(1, (int)(panelW / cellTotal));

    ImGui::BeginChild("##assetGrid", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing() - 4.0f),
                      false, ImGuiWindowFlags_NoScrollbar);
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
                    // DrawList — не создаёт item, не сдвигает курсор, не ломает drag
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
                m_editingMaterialPath = item.path;
                m_showMaterialEditor  = true;
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
            }
        }

        // Drag source — только для .emat
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

        if (hovered && ImGui::BeginTooltip()) {
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

    ImGui::Separator();
    ImGui::TextDisabled("%zu items", m_assetBrowserItems.size());
    if (!m_assetBrowserSelected.empty()) {
        ImGui::SameLine(0, 6);
        ImGui::TextDisabled("|  %s",
            fs::path(m_assetBrowserSelected).filename().string().c_str());
    }

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

    // Mini material preview — справа, через DrawList (не перехватывает ввод)
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

void EditorUI::RenderViewport() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    ImVec2 viewportSize = ImGui::GetContentRegionAvail();
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

        // Drop target: перетаскивание материала на viewport
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ASSET_EMAT")) {
                std::string matPath(static_cast<const char*>(p->Data));

                // Pick объект под курсором
                ImVec2 mousePos = ImGui::GetIO().MousePos;
                ImVec2 vpMin    = imageScreenPos;
                float ndcX = (2.0f * (mousePos.x - vpMin.x)) / viewportSize.x - 1.0f;
                float ndcY = 1.0f - (2.0f * (mousePos.y - vpMin.y)) / viewportSize.y;
                int hitIdx = PickObject(m_gameObjects, view, projection, ndcX, ndcY);

                int targetIdx = (hitIdx >= 0) ? hitIdx : m_selectedObjectIndex;
                if (targetIdx >= 0 && targetIdx < (int)m_gameObjects.size()) {
                    m_gameObjects[targetIdx].materialPath = matPath;
                    m_selectedObjectIndex = targetIdx;
                    m_sceneDirty = true;
                    namespace fs = std::filesystem;
                    LogInfo("Material \"" + fs::path(matPath).stem().string()
                        + "\" → " + m_gameObjects[targetIdx].name);
                }
            }

            // Подсказка во время hover
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
                    // WORLD and LOCAL modes — avoids the mModelSource*T_delta
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
            if (ImGui::Button("New Material")) {
                namespace fs = std::filesystem;
                fs::create_directories("Assets/Materials");
                int idx = 1;
                std::string path;
                do {
                    path = "Assets/Materials/Material_" + std::to_string(idx++) + ".emat";
                } while (fs::exists(path));
                Material mat;
                if (mat.Save(path)) {
                    selected->materialPath = path;
                    MaterialManager::Invalidate(path);
                    m_sceneDirty        = true;
                    m_assetBrowserDirty = true;
                    LogSuccess("Material created: " + fs::path(path).filename().string());
                }
            }
        } else {
            ImGui::TextDisabled("%s",
                std::filesystem::path(selected->materialPath).filename().string().c_str());
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

                ImGui::Spacing();
                if (ImGui::Button("Open Material Editor")) {
                    m_editingMaterialPath  = selected->materialPath;
                    m_showMaterialEditor   = true;
                }

                ImGui::Separator();
                ImGui::Spacing();
                if (ImGui::Button("Remove Material")) {
                    selected->materialPath.clear();
                    m_sceneDirty = true;
                }
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
        ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "Select an object in the Scene Objects panel");
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

        // Use explicit screen positions so stacked buttons don't confuse SameLine
        ImVec2 rowOrigin = ImGui::GetCursorScreenPos();
        for (int i = 0; i < 3; ++i) {
            ImGui::SetCursorScreenPos({rowOrigin.x + i * (fieldW + gap), rowOrigin.y});
            char fid[32]; snprintf(fid, sizeof(fid), "##%s%d", rowLabel, i);
            if (FieldSpinner(fid, &v[i], step, vmin, vmax, fmt, fieldW)) changed = true;
        }
        // Advance cursor past this row
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

void EditorUI::RenderUnsavedChangesDialog() {
    if (m_showUnsavedChangesDialog)
        ImGui::OpenPopup("Несохранённые изменения");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Несохранённые изменения", nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar)) {
        ImGui::Text("В сцене есть несохранённые изменения.");
        ImGui::Text("Сохранить перед закрытием?");
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Сохранить", ImVec2(110, 0))) {
            if (SaveCurrentScene())
                glfwSetWindowShouldClose(m_window, 1);
            m_showUnsavedChangesDialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Закрыть", ImVec2(110, 0))) {
            m_showUnsavedChangesDialog = false;
            ImGui::CloseCurrentPopup();
            glfwSetWindowShouldClose(m_window, 1);
        }
        ImGui::SameLine();
        if (ImGui::Button("Отмена", ImVec2(110, 0))) {
            m_showUnsavedChangesDialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
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
