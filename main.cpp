#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <unordered_map>
#include <ctime>
#include <cmath>
#include "glad/gl.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include "glfw3.h"
#include "glfw3native.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "ImGuizmo.h"
#include "EditorUI.h"
#include "UI/IconsFontAwesome6.h"
#include "UI/EditorStyle.h"
#include "imnodes/imnodes.h"
#include "Core/Branding.h"
#include "Assets/ProjectFile.h"
#include <stb_image.h>

namespace {
    constexpr int kResizeBorder    = 5;
    constexpr int kLauncherTitleH  = 38;   // launcher custom title bar height
    constexpr int kLauncherBtnW    = 46;   // launcher close-button width

    // Window/taskbar icon. Uses branding/icon.png if present; otherwise a
    // procedural rounded crimson square so adding the file later just works.
    void ApplyWindowIcon(GLFWwindow* window) {
        int w = 0, h = 0, ch = 0;
        unsigned char* loaded = stbi_load(branding::kIconPath, &w, &h, &ch, 4);
        std::vector<unsigned char> placeholder;
        unsigned char* pixels = loaded;
        if (!pixels) {
            w = h = 64;
            placeholder.assign((size_t)w * h * 4, 0);
            const float pad = 6.0f, r = 16.0f;
            const float minx = pad, miny = pad, maxx = (float)w - pad, maxy = (float)h - pad;
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x) {
                    float qx = std::max(std::max((minx + r) - x, x - (maxx - r)), 0.0f);
                    float qy = std::max(std::max((miny + r) - y, y - (maxy - r)), 0.0f);
                    if (qx * qx + qy * qy <= r * r) {
                        size_t i = ((size_t)y * w + x) * 4;
                        placeholder[i + 0] = branding::kAccentR;
                        placeholder[i + 1] = branding::kAccentG;
                        placeholder[i + 2] = branding::kAccentB;
                        placeholder[i + 3] = 0xFF;
                    }
                }
            pixels = placeholder.data();
        }
        GLFWimage img; img.width = w; img.height = h; img.pixels = pixels;
        glfwSetWindowIcon(window, 1, &img);
        if (loaded) stbi_image_free(loaded);
    }

    // Shared font setup (main UI font + Font Awesome merge), used by both windows.
    void SetupFonts(ImGuiIO& io) {
        const char* fontCandidates[] = {
            "Assets/Fonts/Ubuntu-Regular.ttf",
            "C:/Windows/Fonts/seguivar.ttf",
            "C:/Windows/Fonts/segoeui.ttf",
            "C:/Windows/Fonts/arial.ttf",
        };
        ImFontConfig fontCfg;
        fontCfg.OversampleH        = 3;
        fontCfg.OversampleV        = 2;
        fontCfg.PixelSnapH         = false;
        fontCfg.RasterizerMultiply = 1.0f;

        bool fontLoaded = false;
        for (const char* path : fontCandidates) {
            if (FILE* f = fopen(path, "rb")) {
                fclose(f);
                io.Fonts->AddFontFromFileTTF(path, 17.0f, &fontCfg,
                    io.Fonts->GetGlyphRangesCyrillic());
                fontLoaded = true;
                break;
            }
        }
        if (!fontLoaded)
            io.Fonts->AddFontDefault();

        const char* faCandidates[] = {
            "Assets/Fonts/fa-solid-900.ttf",
            "Assets/Fonts/fa-solid-900.otf",
            "Assets/Fonts/Font Awesome 6 Free-Solid-900.otf",
            "Assets/Fonts/Font Awesome 6 Free-Solid-900.ttf",
        };
        static const ImWchar fa_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
        ImFontConfig fa_cfg;
        fa_cfg.MergeMode        = true;
        fa_cfg.PixelSnapH       = true;
        fa_cfg.GlyphMinAdvanceX = 16.0f;
        fa_cfg.GlyphMaxAdvanceX = 16.0f;
        for (const char* fa_path : faCandidates) {
            if (FILE* f = fopen(fa_path, "rb")) {
                fclose(f);
                io.Fonts->AddFontFromFileTTF(fa_path, 15.0f, &fa_cfg, fa_ranges);
                break;
            }
        }
    }

    // ── .fcproj file dialogs ─────────────────────────────────────────────────
    std::string OpenProjectDialog() {
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

    std::string SaveProjectDialog() {
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

    // ── Frameless editor window plumbing ─────────────────────────────────────
    WNDPROC     g_originalWndProc = nullptr;
    GLFWwindow* g_glfwWindow      = nullptr;

    LRESULT CALLBACK CustomWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_NCCALCSIZE:
            if (wParam) return 0;
            break;
        case WM_NCACTIVATE:
            return TRUE;
        case WM_NCHITTEST: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            RECT rc;
            GetWindowRect(hwnd, &rc);
            if (!IsZoomed(hwnd) && !IsIconic(hwnd)) {
                bool L = pt.x <  rc.left   + kResizeBorder;
                bool R = pt.x >= rc.right  - kResizeBorder;
                bool T = pt.y <  rc.top    + kResizeBorder;
                bool B = pt.y >= rc.bottom - kResizeBorder;
                if (T && L) return HTTOPLEFT;
                if (T && R) return HTTOPRIGHT;
                if (B && L) return HTBOTTOMLEFT;
                if (B && R) return HTBOTTOMRIGHT;
                if (T)      return HTTOP;
                if (B)      return HTBOTTOM;
                if (L)      return HTLEFT;
                if (R)      return HTRIGHT;
            }
            return HTCLIENT;
        }
        case WM_GETMINMAXINFO: {
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
            mmi->ptMinTrackSize = { 800, 600 };
            HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi{ sizeof(mi) };
            GetMonitorInfo(monitor, &mi);
            mmi->ptMaxPosition = { mi.rcWork.left, mi.rcWork.top };
            mmi->ptMaxSize     = { mi.rcWork.right  - mi.rcWork.left,
                                   mi.rcWork.bottom - mi.rcWork.top };
            return 0;
        }
        }
        return CallWindowProcW(g_originalWndProc, hwnd, msg, wParam, lParam);
    }

    // Launcher window: frameless, fixed size. The title bar (minus the close
    // button) is HTCAPTION so Windows drags it natively; everything else stays
    // client so ImGui handles it. No resize borders.
    WNDPROC g_launcherOrigProc = nullptr;
    LRESULT CALLBACK LauncherWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_NCCALCSIZE:
            if (wParam) return 0;
            break;
        case WM_NCACTIVATE:
            return TRUE;
        case WM_NCHITTEST: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            RECT rc; GetWindowRect(hwnd, &rc);
            int x = pt.x - rc.left;
            int y = pt.y - rc.top;
            int w = rc.right - rc.left;
            if (y >= 0 && y < kLauncherTitleH) {
                if (x >= w - kLauncherBtnW) return HTCLIENT;   // close button → ImGui
                return HTCAPTION;                               // drag region
            }
            return HTCLIENT;
        }
        }
        return CallWindowProcW(g_launcherOrigProc, hwnd, msg, wParam, lParam);
    }

    // Load branding/icon.png as a GL texture in the current context (for the
    // launcher's own header mark). 0 if the file is absent.
    GLuint LoadLauncherLogo() {
        int iw = 0, ih = 0, ic = 0;
        unsigned char* d = stbi_load(branding::kIconPath, &iw, &ih, &ic, 4);
        if (!d) return 0;
        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, iw, ih, 0, GL_RGBA, GL_UNSIGNED_BYTE, d);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(d);
        return tex;
    }

    // ── Phase 1: project launcher window. Returns the chosen .fcproj, or "". ──
    std::string RunProjectPicker() {
        constexpr int kW = 640, kH = 470;
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        GLFWwindow* win = glfwCreateWindow(kW, kH, branding::kAppName, nullptr, nullptr);
        if (!win) return "";
        ApplyWindowIcon(win);

        // Frameless but a "real" window so DWM rounds it; native title-bar drag.
        HWND hwnd = glfwGetWin32Window(win);
        SetWindowLong(hwnd, GWL_STYLE, GetWindowLong(hwnd, GWL_STYLE) | WS_THICKFRAME);
        g_launcherOrigProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(LauncherWndProc)));
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        DWORD corner = 2;  // DWMWCP_ROUND
        DwmSetWindowAttribute(hwnd, 33 /*DWMWA_WINDOW_CORNER_PREFERENCE*/, &corner, sizeof(corner));
        {   // center on the work area
            HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
            MONITORINFO mi{ sizeof(mi) }; GetMonitorInfo(mon, &mi);
            int sw = mi.rcWork.right - mi.rcWork.left, sh = mi.rcWork.bottom - mi.rcWork.top;
            SetWindowPos(hwnd, nullptr, mi.rcWork.left + (sw - kW) / 2, mi.rcWork.top + (sh - kH) / 2,
                0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }

        glfwMakeContextCurrent(win);
        glfwSwapInterval(1);
        gladLoadGL((GLADloadfunc)glfwGetProcAddress);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        SetupFonts(io);
        ApplyEngineStyle();
        ImGui_ImplGlfw_InitForOpenGL(win, true);
        ImGui_ImplOpenGL3_Init("#version 330");

        GLuint logo = LoadLauncherLogo();
        std::vector<projects::RecentEntry> recent = projects::LoadRecent();
        std::string result;
        std::unordered_map<std::string, float> flip;   // path -> 0 (front) .. 1 (back)
        std::string pendingDelete;                      // project pending delete-confirm

        const float titleH = (float)kLauncherTitleH, btnW = (float)kLauncherBtnW;

        while (!glfwWindowShouldClose(win) && result.empty()) {
            glfwPollEvents();
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            int ww = 0, wh = 0;
            glfwGetWindowSize(win, &ww, &wh);
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2((float)ww, (float)wh));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28, 18));   // real content padding
            ImGui::Begin("##launcher", nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus);

            ImVec2 wp = ImGui::GetWindowPos();
            float  fw = ImGui::GetWindowSize().x;
            float  cy = wp.y + titleH * 0.5f;
            ImDrawList* dl = ImGui::GetWindowDrawList();

            // ── Title bar (drawn; drag is native via HTCAPTION) ──────────────
            dl->AddRectFilled(wp, ImVec2(wp.x + fw, wp.y + titleH), IM_COL32(24, 24, 24, 255));
            dl->AddLine(ImVec2(wp.x, wp.y + titleH - 1), ImVec2(wp.x + fw, wp.y + titleH - 1),
                        IM_COL32(255, 255, 255, 20));
            {
                ImVec2 mp0(wp.x + 14, cy - 9), mp1(wp.x + 32, cy + 9);
                if (logo) dl->AddImage((ImTextureID)(intptr_t)logo, mp0, mp1, ImVec2(0, 0), ImVec2(1, 1));
                else      dl->AddRectFilled(mp0, mp1, IM_COL32(branding::kAccentR, branding::kAccentG,
                                                              branding::kAccentB, 255), 4.0f);
            }
            {
                float ty = wp.y + (titleH - ImGui::GetTextLineHeight()) * 0.5f;
                std::string bar = std::string(branding::kAppName) + "  \xe2\x80\x94  Projects";
                dl->AddText(ImVec2(wp.x + 40, ty), IM_COL32(210, 210, 220, 255), bar.c_str());
            }
            // Close button (hit-tested by mouse; its region is HTCLIENT via the WndProc).
            {
                ImVec2 m = ImGui::GetIO().MousePos;
                bool over = (m.x >= wp.x + fw - btnW && m.x < wp.x + fw &&
                             m.y >= wp.y && m.y < wp.y + titleH);
                if (over && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    glfwSetWindowShouldClose(win, 1);
                if (over)
                    dl->AddRectFilled(ImVec2(wp.x + fw - btnW, wp.y), ImVec2(wp.x + fw, wp.y + titleH),
                                      IM_COL32(196, 43, 28, 255));
                float h = 4.0f; ImVec2 cc(wp.x + fw - btnW * 0.5f, cy);
                ImU32 xc = over ? IM_COL32(255, 255, 255, 255) : IM_COL32(155, 155, 168, 255);
                dl->AddLine({ cc.x - h, cc.y - h }, { cc.x + h, cc.y + h }, xc, 1.4f);
                dl->AddLine({ cc.x + h, cc.y - h }, { cc.x - h, cc.y + h }, xc, 1.4f);
            }

            // ── Body: window padding handles left/right; a spacer clears the bar ──
            ImGui::Dummy(ImVec2(0, titleH - ImGui::GetStyle().WindowPadding.y + 6.0f));

            ImGui::SetWindowFontScale(1.45f);
            ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.97f, 1.0f), "%s", branding::kAppName);
            ImGui::SetWindowFontScale(1.0f);
            ImGui::TextDisabled("v%s  \xc2\xb7  select or create a project", branding::kVersion);
            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

            // Only the create button is accented (toned-down crimson); the rest
            // use the shared editor button style.
            ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(192, 54, 76, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(208, 68, 94, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(168, 40, 64, 255));
            if (ImGui::Button("New Project", ImVec2(180, 36))) {
                std::string pf = SaveProjectDialog();
                if (!pf.empty() && projects::Create(pf, std::filesystem::path(pf).stem().string()))
                    result = pf;
            }
            ImGui::PopStyleColor(3);
            ImGui::SameLine();
            if (ImGui::Button("Open Project...", ImVec2(180, 36))) {
                std::string pf = OpenProjectDialog();
                if (!pf.empty()) result = pf;
            }

            ImGui::Spacing();
            ImGui::TextDisabled("Recent projects");
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
            ImGui::BeginChild("##recent", ImVec2(0, 0), true);

            const float nameColX = 14.0f, pathColX = 200.0f;
            ImDrawList* cdl = ImGui::GetWindowDrawList();

            // Column header.
            {
                ImVec2 hp = ImGui::GetCursorScreenPos();
                cdl->AddText(ImVec2(hp.x + nameColX, hp.y), IM_COL32(120, 120, 132, 255), "NAME");
                cdl->AddText(ImVec2(hp.x + pathColX, hp.y), IM_COL32(120, 120, 132, 255), "PATH");
                ImGui::Dummy(ImVec2(0, ImGui::GetTextLineHeight() + 4.0f));
                ImGui::Separator();
                ImGui::Spacing();
            }

            if (recent.empty())
                ImGui::TextDisabled("(none yet)");

            std::string removeReq;   // "remove from recent" target (handled after the loop)

            for (const projects::RecentEntry& entry : recent) {
                const std::string& rp = entry.path;
                ImGui::PushID(rp.c_str());

                const float rowH = 46.0f;
                float  availW = ImGui::GetContentRegionAvail().x;
                ImVec2 p0 = ImGui::GetCursorScreenPos();
                ImVec2 p1 = ImVec2(p0.x + availW, p0.y + rowH);
                float  lineH = ImGui::GetTextLineHeight();

                ImGui::InvisibleButton("##row", ImVec2(availW, rowH));
                bool hover      = ImGui::IsItemHovered();
                bool rowClicked = ImGui::IsItemClicked();
                bool menuOpen   = ImGui::IsPopupOpen("##cardmenu");   // keep flipped while its menu is up

                // Flip: hover (or its open menu) animates 0 (front) -> 1 (back). Slow + smooth.
                float dt = ImGui::GetIO().DeltaTime; if (dt > 0.05f) dt = 0.05f;
                float& fp = flip[rp];
                fp += ((hover || menuOpen) ? 1.0f : -1.0f) * dt * 2.0f;
                if (fp < 0.0f) fp = 0.0f;
                if (fp > 1.0f) fp = 1.0f;
                float e    = fp * fp * (3.0f - 2.0f * fp);       // smoothstep
                float vf   = fabsf(cosf(e * 3.14159265f));       // height factor 1 -> 0 -> 1
                bool  back = (e >= 0.5f);
                int   a    = (int)(vf * vf * 255.0f);            // content fade

                // Flip around the HORIZONTAL axis: squish the card HEIGHT about its centre.
                float cyc = p0.y + rowH * 0.5f;
                float hh  = (rowH * 0.5f) * (vf < 0.03f ? 0.03f : vf);
                ImVec2 cp0(p0.x, cyc - hh), cp1(p1.x, cyc + hh);
                ImU32 bg = back  ? IM_COL32(54, 44, 58, 255)
                         : hover ? IM_COL32(60, 46, 54, 255)
                                 : IM_COL32(40, 40, 46, 255);
                cdl->AddRectFilled(cp0, cp1, bg, 6.0f);
                // Border tracks the flip amount (not hover/menu booleans) so it can't
                // blink off on the single frame between losing hover and the menu opening.
                if (fp > 0.02f) cdl->AddRect(cp0, cp1, IM_COL32(192, 54, 76, 170), 6.0f, 0, 1.0f);

                float ty = cyc - lineH * 0.5f;
                bool  overGear = false;
                if (!back) {
                    // ── Front: name + path ──
                    std::string name = std::filesystem::path(rp).stem().string();
                    cdl->PushClipRect(ImVec2(p0.x + nameColX, cp0.y), ImVec2(p0.x + pathColX - 8.0f, cp1.y), true);
                    cdl->AddText(ImVec2(p0.x + nameColX, ty), IM_COL32(224, 224, 232, a), name.c_str());
                    cdl->PopClipRect();
                    cdl->PushClipRect(ImVec2(p0.x + pathColX, cp0.y), ImVec2(p1.x - 10.0f, cp1.y), true);
                    cdl->AddText(ImVec2(p0.x + pathColX, ty), IM_COL32(132, 132, 142, a), rp.c_str());
                    cdl->PopClipRect();
                } else {
                    // ── Back: last opened + settings gear ──
                    char when[64] = "unknown";
                    if (entry.opened > 0) {
                        std::time_t t = (std::time_t)entry.opened;
                        std::tm tmv{};
                        localtime_s(&tmv, &t);
                        std::strftime(when, sizeof(when), "%b %d, %Y   %H:%M", &tmv);
                    }
                    std::string ds = std::string("Last opened:   ") + when;
                    cdl->PushClipRect(ImVec2(p0.x + nameColX, cp0.y), ImVec2(p1.x - 46.0f, cp1.y), true);
                    cdl->AddText(ImVec2(p0.x + nameColX, ty), IM_COL32(200, 200, 210, a), ds.c_str());
                    cdl->PopClipRect();

                    // Settings gear — manual hit-test so it doesn't fight the card button.
                    ImVec2 gc(p1.x - 26.0f, cyc);
                    ImVec2 m  = ImGui::GetIO().MousePos;
                    overGear  = (m.x >= gc.x - 16.0f && m.x <= gc.x + 16.0f && m.y >= p0.y && m.y <= p1.y);
                    if (overGear && !menuOpen && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                        ImGui::OpenPopup("##cardmenu");
                    ImVec2 gsz = ImGui::CalcTextSize(ICON_FA_GEAR);
                    ImU32  gcol = overGear ? IM_COL32(255, 255, 255, a) : IM_COL32(175, 175, 185, a);
                    cdl->AddText(ImVec2(gc.x - gsz.x * 0.5f, gc.y - gsz.y * 0.5f), gcol, ICON_FA_GEAR);
                }

                if (ImGui::BeginPopup("##cardmenu")) {
                    // Rounded hover highlight, matching the editor's top menus.
                    auto rItem = [](const char* label, ImDrawFlags rf) -> bool {
                        ImDrawList* d = ImGui::GetWindowDrawList();
                        d->ChannelsSplit(2);
                        d->ChannelsSetCurrent(1);
                        ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0, 0, 0, 0));
                        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));
                        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  ImVec4(0, 0, 0, 0));
                        bool   clicked = ImGui::MenuItem(label);
                        bool   hov = ImGui::IsItemHovered(), act = ImGui::IsItemActive();
                        ImVec2 rMin = ImGui::GetItemRectMin(), rMax = ImGui::GetItemRectMax();
                        ImGui::PopStyleColor(3);
                        if (hov || act) {
                            d->ChannelsSetCurrent(0);
                            ImU32 col = act ? ImGui::GetColorU32(ImGuiCol_HeaderActive)
                                            : ImGui::GetColorU32(ImGuiCol_HeaderHovered);
                            d->AddRectFilled(rMin, rMax, col, 5.0f, rf);
                        }
                        d->ChannelsMerge();
                        return clicked;
                    };
                    if (rItem("Remove from recent", ImDrawFlags_RoundCornersTop)) removeReq = rp;
                    if (rItem("Delete project (all files)", ImDrawFlags_RoundCornersBottom)) pendingDelete = rp;
                    ImGui::EndPopup();
                }

                // A click anywhere on the card opens it (except on the gear).
                if (rowClicked && !overGear) result = rp;

                ImGui::Dummy(ImVec2(0, 6));
                ImGui::PopID();
            }

            if (!removeReq.empty()) {
                projects::RemoveRecent(removeReq);
                recent = projects::LoadRecent();
            }

            ImGui::EndChild();
            ImGui::PopStyleVar();

            ImGui::End();
            ImGui::PopStyleVar(2);

            // ── Delete-project confirmation (destructive) ────────────────────
            if (!pendingDelete.empty()) {
                ImGui::OpenPopup("Delete Project?");
                ImVec2 c = ImGui::GetMainViewport()->GetCenter();
                ImGui::SetNextWindowPos(c, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                ImGui::SetNextWindowSize(ImVec2(440, 0), ImGuiCond_Always);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22, 18));
                if (ImGui::BeginPopupModal("Delete Project?", nullptr,
                        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
                    namespace fs = std::filesystem;
                    std::string nm  = fs::path(pendingDelete).stem().string();
                    std::string dir = fs::path(pendingDelete).parent_path().string();
                    ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.97f, 1.0f), "Delete \"%s\"?", nm.c_str());
                    ImGui::Spacing();
                    ImGui::TextWrapped("This permanently deletes the project folder and EVERYTHING "
                                       "inside it (scenes, assets, ...):");
                    ImGui::TextDisabled("%s", dir.c_str());
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.90f, 0.45f, 0.45f, 1.0f), "This cannot be undone.");
                    ImGui::Spacing(); ImGui::Spacing();

                    ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(176, 40, 40, 255));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(200, 52, 52, 255));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(150, 30, 30, 255));
                    if (ImGui::Button("Delete", ImVec2(120, 0))) {
                        std::error_code ec;
                        fs::remove_all(dir, ec);
                        projects::RemoveRecent(pendingDelete);
                        recent = projects::LoadRecent();
                        pendingDelete.clear();
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::PopStyleColor(3);
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                        pendingDelete.clear();
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopStyleVar();
            }

            glClearColor(0.118f, 0.118f, 0.118f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(win);
        }

        if (logo) glDeleteTextures(1, &logo);
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(win);
        return result;
    }

    // ── Phase 2: the editor, opened on the chosen project. ───────────────────
    void RunEditor(const std::string& projectPath) {
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        GLFWwindow* window = glfwCreateWindow(1280, 800, branding::kAppName, nullptr, nullptr);
        if (!window) return;
        ApplyWindowIcon(window);

        HWND hwnd = glfwGetWin32Window(window);
        LONG style = GetWindowLong(hwnd, GWL_STYLE);
        SetWindowLong(hwnd, GWL_STYLE, style | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX);
        g_originalWndProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(CustomWndProc)));
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        ShowWindow(hwnd, SW_MAXIMIZE);
        g_glfwWindow = window;

        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);
        if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) return;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.IniFilename  = nullptr;
        SetupFonts(io);

        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330");
        ImNodes::SetImGuiContext(ImGui::GetCurrentContext());

        EditorUI editorUI;
        if (!editorUI.Initialize(window)) return;
        editorUI.LoadProject(projectPath);

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            static bool s_wasIconified = false;
            static int  s_settleFrames = 0;
            const  bool s_isIconified  = glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0;

            if (s_wasIconified && !s_isIconified) {
                int fbW = 0, fbH = 0;
                glfwGetFramebufferSize(window, &fbW, &fbH);
                if (fbW > 0 && fbH > 0)
                    glViewport(0, 0, fbW, fbH);
                s_settleFrames = 4;
            }
            s_wasIconified = s_isIconified;

            if (s_isIconified || s_settleFrames > 0) {
                if (!s_isIconified && s_settleFrames > 0) --s_settleFrames;
                Sleep(8);
                continue;
            }

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            ImGuizmo::BeginFrame();

            editorUI.Render();

            glClearColor(0.118f, 0.118f, 0.118f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);

            if (editorUI.m_pendingMinimize) {
                editorUI.m_pendingMinimize = false;
                s_settleFrames = 0;
                ::ShowWindow(hwnd, SW_MINIMIZE);
            }
            if (editorUI.m_pendingMaximize) {
                editorUI.m_pendingMaximize = false;
                ::ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
                int fbW = 0, fbH = 0;
                glfwGetFramebufferSize(window, &fbW, &fbH);
                if (fbW > 0 && fbH > 0) glViewport(0, 0, fbW, fbH);
            }
        }

        editorUI.Shutdown();
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
    }
}

static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

int main() {
#ifdef ENGINE_SOURCE_DIR
    SetCurrentDirectoryA(ENGINE_SOURCE_DIR);
#endif
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Launcher first; the editor opens only once a project is chosen.
    std::string project = RunProjectPicker();
    if (!project.empty())
        RunEditor(project);

    glfwTerminate();
    return 0;
}
