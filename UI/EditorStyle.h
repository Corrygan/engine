#pragma once
#include "imgui.h"

// The shared dark theme (VS-style grays + crimson accent). Applied by both the
// editor window and the project launcher so they look identical.
inline void ApplyEngineStyle() {
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();

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
}
