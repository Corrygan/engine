#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <iostream>
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
#include "imnodes/imnodes.h"

namespace {
    constexpr int kTitleBarH    = 32;
    constexpr int kBtnWidth     = 46;
    constexpr int kResizeBorder = 5;

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
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(1280, 800, "Engine", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }

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

    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) return -1;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename  = nullptr;

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

    {
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
        bool faLoaded = false;
        for (const char* fa_path : faCandidates) {
            if (FILE* f = fopen(fa_path, "rb")) {
                fclose(f);
                ImFont* loaded = io.Fonts->AddFontFromFileTTF(fa_path, 15.0f, &fa_cfg, fa_ranges);
                faLoaded = (loaded != nullptr);
                if (!faLoaded)
                    std::cerr << "[Font] FA parse failed: " << fa_path << std::endl;
                break;
            }
        }
        if (!faLoaded)
            std::cerr << "[Font] FA Solid not found — put fa-solid-900.ttf in Assets/Fonts/" << std::endl;
    }

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    // imnodes global init (per-editor contexts created separately)
    ImNodes::SetImGuiContext(ImGui::GetCurrentContext());

    EditorUI editorUI;
    if (!editorUI.Initialize(window)) return -1;

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
    glfwTerminate();
    return 0;
}
