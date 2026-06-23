#include "MaterialNodeEditor.h"
#include "../imnodes/imnodes.h"
#include "../Renderer/Shader.h"
#include "../Renderer/Material.h"
#include "../Renderer/MaterialManager.h"
#include "../Renderer/MaterialPreviewRenderer.h"
#include "../Renderer/SceneRenderer.h"
#include "../Renderer/Texture.h"
#include "../Renderer/TextureManager.h"
#include "IconsFontAwesome6.h"
#include "imgui.h"
#include <filesystem>
#include <algorithm>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>

namespace {
    std::string OpenTextureDlg() {
        OPENFILENAMEA ofn{};
        char buf[MAX_PATH] = "";
        ofn.lStructSize  = sizeof(ofn);
        ofn.lpstrFile    = buf;
        ofn.nMaxFile     = MAX_PATH;
        ofn.lpstrFilter  = "Images\0*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.hdr\0All\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.Flags        = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
        return GetOpenFileNameA(&ofn) ? std::string(buf) : "";
    }
}

namespace fs = std::filesystem;

// ─── Pin colour by type ────────────────────────────────────────────────────
static ImU32 PinColor(PinType t) {
    switch (t) {
        case PinType::Float: return IM_COL32(160,160,160,255);
        case PinType::Vec2:  return IM_COL32(100,200,100,255);
        case PinType::Vec3:  return IM_COL32(220,140, 60,255);
        case PinType::Vec4:  return IM_COL32(120,180,255,255);
    }
    return IM_COL32(200,200,200,255);
}

// ─── Lifecycle ─────────────────────────────────────────────────────────────

MaterialNodeEditor::MaterialNodeEditor() {
    m_imnodesCtx = ImNodes::CreateContext();
}

MaterialNodeEditor::~MaterialNodeEditor() {
    // m_compiledShader is owned by SceneRenderer, do not delete here.
    if (m_imnodesCtx) ImNodes::DestroyContext((ImNodesContext*)m_imnodesCtx);
}

void MaterialNodeEditor::Open(const std::string& matPath, MaterialPreviewRenderer* preview,
                              SceneRenderer* sceneRenderer) {
    m_matPath       = matPath;
    m_graphPath     = matPath + ".graph";
    m_preview       = preview;
    m_sceneRenderer = sceneRenderer;
    m_open      = true;
    m_dirty     = false;
    m_selectedNode = -1;

    // Sync editable name field with the file's stem.
    {
        std::string stem = fs::path(m_matPath).stem().string();
        std::snprintf(m_nameBuf, sizeof(m_nameBuf), "%s", stem.c_str());
    }

    ImNodes::SetCurrentContext((ImNodesContext*)m_imnodesCtx);
    ImNodes::ClearNodeSelection();
    ImNodes::ClearLinkSelection();

    LoadGraph();

    // Style to match engine dark theme
    ImNodesStyle& style = ImNodes::GetStyle();
    style.NodeCornerRounding    = 6.0f;
    style.NodePadding           = ImVec2(8.0f, 4.0f);
    style.NodeBorderThickness   = 1.5f;
    style.LinkThickness         = 2.5f;
    style.LinkLineSegmentsPerLength = 0.1f;
    style.PinCircleRadius       = 5.0f;

    ImNodes::PushColorStyle(ImNodesCol_NodeBackground,        IM_COL32( 38, 38, 42,255));
    ImNodes::PushColorStyle(ImNodesCol_NodeBackgroundHovered, IM_COL32( 45, 45, 50,255));
    ImNodes::PushColorStyle(ImNodesCol_NodeBackgroundSelected,IM_COL32( 50, 50, 60,255));
    ImNodes::PushColorStyle(ImNodesCol_NodeOutline,           IM_COL32( 80, 80, 90,255));
    ImNodes::PushColorStyle(ImNodesCol_TitleBar,              IM_COL32( 55, 55, 65,255));
    ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered,       IM_COL32( 65, 65, 80,255));
    ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected,      IM_COL32(120, 50, 80,255));
    ImNodes::PushColorStyle(ImNodesCol_GridBackground,        IM_COL32( 25, 25, 28,255));
    ImNodes::PushColorStyle(ImNodesCol_GridLine,              IM_COL32( 40, 40, 45,255));
    ImNodes::PushColorStyle(ImNodesCol_GridLinePrimary,       IM_COL32( 55, 55, 60,255));
    ImNodes::PushColorStyle(ImNodesCol_Link,                  IM_COL32(192, 54, 76,255));
    ImNodes::PushColorStyle(ImNodesCol_LinkHovered,           IM_COL32(220, 80,110,255));
    ImNodes::PushColorStyle(ImNodesCol_LinkSelected,          IM_COL32(255,100,130,255));

    // Compile immediately so the object + preview reflect the graph on open.
    TryCompile();
}

void MaterialNodeEditor::EnsureCompiled(const std::string& matPath, SceneRenderer* sr) {
    std::string graphPath = matPath + ".graph";
    if (!fs::exists(graphPath)) return;          // no graph — use default PBR
    // Don't clobber the graph currently being edited.
    if (m_open) return;

    // Compile into a temporary graph without touching the editor's own state.
    MaterialGraph tmp;
    tmp.Load(graphPath);
    if (tmp.nodes.empty()) return;

    CompiledShader compiled = NodeCompiler::Compile(tmp);
    if (!compiled.valid) { if (sr) sr->UnregisterNodeShader(matPath); return; }

    static const char* kVert = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in vec3 aTangent;
layout(location=4) in vec3 aBitangent;
uniform mat4 uModel; uniform mat4 uView; uniform mat4 uProj;
out vec3 vNormal; out vec3 vWorldPos; out vec2 vUV; out vec3 vTangent; out vec3 vBitangent;
const float PI = 3.14159265359;
void main(){
    vec4 wp = uModel*vec4(aPos,1.0);
    vWorldPos = wp.xyz;
    vNormal   = mat3(transpose(inverse(uModel)))*aNormal;
    vTangent  = mat3(uModel)*aTangent;
    vBitangent= mat3(uModel)*aBitangent;
    if (aUV.x==0.0 && aUV.y==0.0) { vec3 n=normalize(aPos);
        vUV=vec2(atan(n.z,n.x)/(2.0*PI)+0.5, acos(clamp(n.y,-1.0,1.0))/PI); }
    else vUV = aUV;
    gl_Position = uProj*uView*wp;
}
)";
    Shader* shader = new Shader(kVert, compiled.fragSrc);
    if (shader->GetID() == 0) { delete shader; if (sr) sr->UnregisterNodeShader(matPath); return; }

    std::vector<std::pair<std::string, std::string>> texBindings;
    for (auto& [nodeId, uname] : compiled.texUniforms) {
        GraphNode* node = tmp.FindNode(nodeId);
        if (node && node->kind == NodeKind::Texture2D)
            texBindings.emplace_back(uname, node->texPath);
    }
    if (sr) sr->RegisterNodeShader(matPath, shader, std::move(texBindings));
    else    delete shader;
}

void MaterialNodeEditor::LoadGraph() {
    m_graph.nodes.clear();
    m_graph.links.clear();
    m_graph.nextId = 1;

    if (!m_graph.Load(m_graphPath) || m_graph.nodes.empty()) {
        m_graph.nodes.clear();
        m_graph.links.clear();
        m_graph.nextId = 1;
        m_graph.AddDefaultOutput();
    }

    // Restore node positions to imnodes
    ImNodes::SetCurrentContext((ImNodesContext*)m_imnodesCtx);
    for (auto& n : m_graph.nodes)
        ImNodes::SetNodeGridSpacePos(n.id, n.pos);

    TryCompile();
}

// ─── Compile ───────────────────────────────────────────────────────────────

void MaterialNodeEditor::TryCompile() {
    m_dirty = false;
    m_compiledShader = nullptr;  // non-owning; SceneRenderer owns node shaders

    m_compiled = NodeCompiler::Compile(m_graph);

    if (!m_compiled.valid) {
        if (m_sceneRenderer) m_sceneRenderer->UnregisterNodeShader(m_matPath);
        return;
    }

    // Vertex shader. vUV falls back to spherical mapping for meshes without UVs.
    static const char* kVert = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in vec3 aTangent;
layout(location=4) in vec3 aBitangent;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
out vec3 vNormal;
out vec3 vWorldPos;
out vec2 vUV;
out vec3 vTangent;
out vec3 vBitangent;
const float PI = 3.14159265359;
void main(){
    vec4 wp = uModel*vec4(aPos,1.0);
    vWorldPos  = wp.xyz;
    vNormal    = mat3(transpose(inverse(uModel)))*aNormal;
    vTangent   = mat3(uModel)*aTangent;
    vBitangent = mat3(uModel)*aBitangent;
    if (aUV.x == 0.0 && aUV.y == 0.0) {
        vec3 n = normalize(aPos);
        vUV = vec2(atan(n.z, n.x)/(2.0*PI)+0.5, acos(clamp(n.y,-1.0,1.0))/PI);
    } else {
        vUV = aUV;
    }
    gl_Position = uProj*uView*wp;
}
)";
    Shader* shader = new Shader(kVert, m_compiled.fragSrc);
    if (shader->GetID() == 0) {
        m_compiled.valid = false;
        m_compiled.error = "GLSL link failed (check console)";
        delete shader;
        if (m_sceneRenderer) m_sceneRenderer->UnregisterNodeShader(m_matPath);
        return;
    }

    // Build texture bindings: uniformName -> texPath (from Texture2D nodes)
    std::vector<std::pair<std::string, std::string>> texBindings;
    for (auto& [nodeId, uname] : m_compiled.texUniforms) {
        GraphNode* node = m_graph.FindNode(nodeId);
        if (node && node->kind == NodeKind::Texture2D)
            texBindings.emplace_back(uname, node->texPath);
    }

    // Refresh the cached thumbnail so node edits show up on the preview sphere
    // (asset browser, inspector swatch). Do this before texBindings is moved.
    if (m_preview)
        m_preview->UpdatePreviewWithShader(m_matPath, shader, texBindings);

    if (m_sceneRenderer) {
        m_compiledShader = shader;  // reference only
        m_sceneRenderer->RegisterNodeShader(m_matPath, shader, std::move(texBindings));
    } else {
        delete shader;  // nowhere to use it
    }
    m_previewTex = 0;
}

void MaterialNodeEditor::SaveGraph() {
    // Save node positions from imnodes
    ImNodes::SetCurrentContext((ImNodesContext*)m_imnodesCtx);
    for (auto& n : m_graph.nodes)
        n.pos = ImNodes::GetNodeGridSpacePos(n.id);
    m_graph.Save(m_graphPath);
}

void MaterialNodeEditor::CommitNow() {
    // Discrete edit (add/delete node, link change, texture pick): apply now.
    m_dirty = false;
    m_compileTimer = 0.0f;
    TryCompile();   // recompile → re-register scene shader + refresh preview
    SaveGraph();    // autosave graph to disk
}

void MaterialNodeEditor::Close() {
    if (m_open) {
        TryCompile();   // make sure latest in-memory edits are applied
        SaveGraph();    // never lose unsaved work on close
    }
    m_open = false;
}

void MaterialNodeEditor::RenameTo(const std::string& rawStem) {
    auto revert = [&]{
        std::string cur = fs::path(m_matPath).stem().string();
        std::snprintf(m_nameBuf, sizeof(m_nameBuf), "%s", cur.c_str());
    };

    // Trim whitespace.
    std::string stem = rawStem;
    size_t a = stem.find_first_not_of(" \t");
    size_t b = stem.find_last_not_of(" \t");
    stem = (a == std::string::npos) ? "" : stem.substr(a, b - a + 1);

    std::string curStem = fs::path(m_matPath).stem().string();
    if (stem.empty() || stem == curStem) { revert(); return; }

    // Reject illegal filename characters.
    if (stem.find_first_of("<>:\"/\\|?*") != std::string::npos) { revert(); return; }

    fs::path dir    = fs::path(m_matPath).parent_path();
    fs::path newMat = dir / (stem + ".emat");

    std::error_code ec;
    if (fs::exists(newMat, ec)) { revert(); return; }   // name already taken

    // Flush current graph to the old .graph before moving files.
    SaveGraph();

    std::string oldMat   = m_matPath;
    std::string oldGraph = m_graphPath;
    std::string newMatS  = newMat.string();
    std::string newGraph = newMatS + ".graph";

    fs::rename(oldMat, newMatS, ec);
    if (ec) { revert(); return; }                       // rename failed — keep old
    fs::rename(oldGraph,        newGraph,        ec);    // best-effort for sidecars
    fs::rename(oldMat + ".meta", newMatS + ".meta", ec);

    // Re-point engine state from old path → new path.
    MaterialManager::Invalidate(oldMat);
    MaterialManager::Invalidate(newMatS);
    if (m_preview) m_preview->InvalidatePreview(oldMat);

    m_matPath   = newMatS;
    m_graphPath = newGraph;
    std::snprintf(m_nameBuf, sizeof(m_nameBuf), "%s", stem.c_str());

    // Register the compiled shader under the new path, then free the old one.
    TryCompile();
    if (m_sceneRenderer) m_sceneRenderer->UnregisterNodeShader(oldMat);

    // Let the owner re-point GameObjects + refresh the asset browser.
    if (m_onRename) m_onRename(oldMat, newMatS);
}

// ─── Render ────────────────────────────────────────────────────────────────

void MaterialNodeEditor::Render() {
    if (!m_open) return;

    ImNodes::SetCurrentContext((ImNodesContext*)m_imnodesCtx);

    // Auto-compile + autosave after a short debounce (covers continuous drags)
    if (m_dirty) {
        m_compileTimer += ImGui::GetIO().DeltaTime;
        if (m_compileTimer >= kCompileDelay) {
            m_compileTimer = 0.0f;
            TryCompile();   // sets m_dirty=false; updates scene + preview
            SaveGraph();    // autosave
        }
    }

    ImGui::SetNextWindowSize(ImVec2(1200.0f, 780.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                            ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse  | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoResize    | ImGuiWindowFlags_NoDocking   |
        ImGuiWindowFlags_NoTitleBar;
    if (!ImGui::Begin("##NodeEditor", nullptr, flags)) {
        ImGui::End(); ImGui::PopStyleVar(); Close(); return;
    }

    // ── Custom title bar ─────────────────────────────────────────────────
    {
        std::string fname = fs::path(m_matPath).filename().string();
        ImGui::Text("%s", fname.c_str());
        ImGui::SameLine(0, 6);
        ImGui::TextDisabled("[Graph]");

        // Centered close button via DrawList (same as IconButton in EditorUI)
        constexpr float kSz = 22.0f;
        ImGui::SameLine(ImGui::GetContentRegionMax().x - kSz);
        ImGui::InvisibleButton("##closenode", ImVec2(kSz, kSz));
        bool hov = ImGui::IsItemHovered();
        bool act = ImGui::IsItemActive();
        if (ImGui::IsItemClicked()) Close();
        ImVec2 bMin = ImGui::GetItemRectMin();
        ImVec2 bMax = ImGui::GetItemRectMax();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (act)       dl->AddCircleFilled({bMin.x+kSz*.5f,bMin.y+kSz*.5f}, kSz*.5f, IM_COL32(80,80,90,200));
        else if (hov)  dl->AddCircleFilled({bMin.x+kSz*.5f,bMin.y+kSz*.5f}, kSz*.5f, IM_COL32(60,60,70,180));
        ImVec2 ts = ImGui::CalcTextSize(ICON_FA_XMARK);
        dl->AddText({bMin.x+(kSz-ts.x)*.5f, bMin.y+(kSz-ts.y)*.5f},
                    IM_COL32(200,200,200,255), ICON_FA_XMARK);
    }
    ImGui::Separator();

    RenderToolbar();
    ImGui::Separator();

    float sideW = 240.0f;
    float canvW = ImGui::GetContentRegionAvail().x - sideW - 6.0f;
    float h     = ImGui::GetContentRegionAvail().y;

    // Canvas
    ImGui::BeginChild("##canvas", ImVec2(canvW, h), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    RenderCanvas();
    ImGui::EndChild();

    ImGui::SameLine(0, 6);

    // Sidebar
    ImGui::BeginChild("##sidebar", ImVec2(sideW, h), false);
    RenderSidebar();
    ImGui::EndChild();

    ImGui::End();
    ImGui::PopStyleVar();
}

void MaterialNodeEditor::RenderToolbar() {
    // Editable material name — renames the .emat (+ sidecars) on commit.
    ImGui::TextUnformatted("Name");
    ImGui::SameLine(0, 8);
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputText("##matname", m_nameBuf, sizeof(m_nameBuf),
                     ImGuiInputTextFlags_EnterReturnsTrue);
    if (ImGui::IsItemDeactivatedAfterEdit())
        RenameTo(m_nameBuf);

    // Compile error (if any) — autosave/compile happen automatically.
    if (!m_compiled.valid && !m_compiled.error.empty()) {
        ImGui::SameLine(0, 16);
        ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f),
                           "Error: %s", m_compiled.error.c_str());
    }

    ImGui::SameLine(ImGui::GetContentRegionMax().x - 100.0f);
    ImGui::TextDisabled("%zu nodes", m_graph.nodes.size());
}

void MaterialNodeEditor::RenderCanvas() {
    ImNodes::BeginNodeEditor();

    // Draw nodes
    for (auto& node : m_graph.nodes) {
        // Title bar color for MaterialOutput
        if (node.kind == NodeKind::MaterialOutput)
            ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(80, 40,120,255));

        ImNodes::BeginNode(node.id);
        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted(node.Title());
        ImNodes::EndNodeTitleBar();

        // Input pins (EnableLinkDetachWithDragClick = тяни пин чтобы оторвать связь)
        for (auto& pin : node.inputs) {
            ImNodes::PushColorStyle(ImNodesCol_Pin,        PinColor(pin.type));
            ImNodes::PushColorStyle(ImNodesCol_PinHovered, PinColor(pin.type));
            ImNodes::PushAttributeFlag(ImNodesAttributeFlags_EnableLinkDetachWithDragClick);
            ImNodes::BeginInputAttribute(pin.id, ImNodesPinShape_CircleFilled);
            ImGui::TextDisabled("%s", pin.name.c_str());
            ImNodes::EndInputAttribute();
            ImNodes::PopAttributeFlag();
            ImNodes::PopColorStyle();
            ImNodes::PopColorStyle();
        }

        // Node-specific inline widgets
        ImGui::PushItemWidth(120.0f);
        switch (node.kind) {
            case NodeKind::Color:
                if (ImGui::ColorEdit3("##col", node.color,
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel))
                    m_dirty = true, m_compileTimer = 0.0f;
                break;
            case NodeKind::Float:
                if (ImGui::DragFloat("##fval", &node.floatVal, 0.01f, 0.0f, 1.0f, "%.3f"))
                    m_dirty = true, m_compileTimer = 0.0f;
                break;
            case NodeKind::Texture2D: {
                if (!node.texPath.empty()) {
                    Texture* t = TextureManager::GetOrLoad(node.texPath);
                    if (t) {
                        ImGui::Image((ImTextureID)(intptr_t)t->GetID(),
                                     ImVec2(80, 80), ImVec2(0,1), ImVec2(1,0));
                    } else {
                        ImGui::TextDisabled("%s", fs::path(node.texPath).filename().string().c_str());
                    }
                } else {
                    ImGui::TextDisabled("(no texture)");
                }
                break;
            }
            default: break;
        }
        ImGui::PopItemWidth();

        // Output pins
        for (auto& pin : node.outputs) {
            ImNodes::PushColorStyle(ImNodesCol_Pin,        PinColor(pin.type));
            ImNodes::PushColorStyle(ImNodesCol_PinHovered, PinColor(pin.type));
            ImNodes::BeginOutputAttribute(pin.id, ImNodesPinShape_CircleFilled);
            float tw = ImGui::CalcTextSize(pin.name.c_str()).x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (80.0f - tw));
            ImGui::TextDisabled("%s", pin.name.c_str());
            ImNodes::EndOutputAttribute();
            ImNodes::PopColorStyle();
            ImNodes::PopColorStyle();
        }

        ImNodes::EndNode();

        if (node.kind == NodeKind::MaterialOutput)
            ImNodes::PopColorStyle();
    }

    // Draw links
    for (auto& link : m_graph.links)
        ImNodes::Link(link.id, link.fromPin, link.toPin);

    ImNodes::EndNodeEditor();

    // ── All imnodes queries MUST be after EndNodeEditor ──────────────────
    // Save node positions
    for (auto& n : m_graph.nodes)
        n.pos = ImNodes::GetNodeGridSpacePos(n.id);

    // Right-click → add node menu (IsNodeHovered/IsLinkHovered only valid here)
    {
        int hovNode = -1, hovLink = -1;
        bool onNode = ImNodes::IsNodeHovered(&hovNode);
        bool onLink = ImNodes::IsLinkHovered(&hovLink);
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup
                                   | ImGuiHoveredFlags_ChildWindows)
            && ImGui::IsMouseClicked(ImGuiMouseButton_Right)
            && !onNode && !onLink)
        {
            ImVec2 mp = ImGui::GetMousePos();
            m_addMenuX = mp.x;
            m_addMenuY = mp.y;
            ImGui::OpenPopup("##addNode");
        }
    }
    RenderAddNodeMenu();

    // Handle new link
    int startPin, endPin;
    if (ImNodes::IsLinkCreated(&startPin, &endPin)) {
        // Validate: endPin must be an input pin
        GraphNode* toNode = m_graph.FindNodeByInputPin(endPin);
        if (toNode) {
            // Remove existing link to that pin
            auto it = std::find_if(m_graph.links.begin(), m_graph.links.end(),
                [&](const GraphLink& l){ return l.toPin == endPin; });
            if (it != m_graph.links.end()) m_graph.links.erase(it);

            GraphLink lnk;
            lnk.id      = m_graph.NewId();
            lnk.fromPin = startPin;
            lnk.toPin   = endPin;
            m_graph.links.push_back(lnk);
            CommitNow();
        }
    }

    // Handle destroyed link (Delete key or detach-drag)
    int destroyedLink;
    if (ImNodes::IsLinkDestroyed(&destroyedLink)) {
        auto it = std::find_if(m_graph.links.begin(), m_graph.links.end(),
            [&](const GraphLink& l){ return l.id == destroyedLink; });
        if (it != m_graph.links.end()) m_graph.links.erase(it);
        CommitNow();
    }

    // Handle dropped detached link (drag-detach dropped without reconnecting)
    int droppedPin;
    if (ImNodes::IsLinkDropped(&droppedPin, true)) {
        // Remove link that was attached to droppedPin
        m_graph.links.erase(std::remove_if(m_graph.links.begin(), m_graph.links.end(),
            [droppedPin](const GraphLink& l){
                return l.fromPin == droppedPin || l.toPin == droppedPin;
            }), m_graph.links.end());
        CommitNow();
    }

    // Delete selected nodes/links — guard against WantTextInput (DragFloat etc.)
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && !ImGui::GetIO().WantTextInput) {
        int selCount = ImNodes::NumSelectedNodes();
        if (selCount > 0) {
            std::vector<int> selIds(selCount);
            ImNodes::GetSelectedNodes(selIds.data());
            for (int sid : selIds) {
                auto* n = m_graph.FindNode(sid);
                if (!n || n->kind == NodeKind::MaterialOutput) continue;
                // Remove links connected to this node's pins
                auto allPins = n->inputs;
                for (auto& p : n->outputs) allPins.push_back(p);
                m_graph.links.erase(std::remove_if(m_graph.links.begin(), m_graph.links.end(),
                    [&](const GraphLink& l){
                        for (auto& p : allPins) if (l.fromPin==p.id||l.toPin==p.id) return true;
                        return false;
                    }), m_graph.links.end());
                m_graph.nodes.erase(std::find_if(m_graph.nodes.begin(), m_graph.nodes.end(),
                    [sid](const GraphNode& nd){ return nd.id == sid; }));
            }
            CommitNow();
        }
        int selLinkCount = ImNodes::NumSelectedLinks();
        if (selLinkCount > 0) {
            std::vector<int> selLinks(selLinkCount);
            ImNodes::GetSelectedLinks(selLinks.data());
            for (int lid : selLinks) {
                m_graph.links.erase(std::remove_if(m_graph.links.begin(), m_graph.links.end(),
                    [lid](const GraphLink& l){ return l.id == lid; }), m_graph.links.end());
            }
            CommitNow();
        }
    }

    // Track selected node for sidebar
    if (ImNodes::NumSelectedNodes() == 1) {
        int sel[1]; ImNodes::GetSelectedNodes(sel);
        m_selectedNode = sel[0];
    } else {
        m_selectedNode = -1;
    }
}

void MaterialNodeEditor::RenderAddNodeMenu() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(160, 0), ImVec2(220, FLT_MAX));
    if (ImGui::BeginPopup("##addNode")) {
        // screen → grid: grid = screen - canvas_origin - panning
        ImVec2 canvasOrigin = ImGui::GetWindowPos();
        ImVec2 panning      = ImNodes::EditorContextGetPanning();
        ImVec2 clickPos     = ImVec2(m_addMenuX - canvasOrigin.x - panning.x,
                                     m_addMenuY - canvasOrigin.y - panning.y);

        auto addItem = [&](const char* label, NodeKind kind) {
            if (ImGui::MenuItem(label)) {
                m_graph.AddNode(kind, clickPos);
                ImNodes::SetNodeGridSpacePos(m_graph.nodes.back().id, clickPos);
                CommitNow();
                ImGui::CloseCurrentPopup();
            }
        };

        ImGui::TextDisabled("Constants");
        ImGui::Separator();
        addItem("Color",     NodeKind::Color);
        addItem("Float",     NodeKind::Float);
        addItem("Texture2D", NodeKind::Texture2D);
        addItem("UV",        NodeKind::UV);

        ImGui::Spacing();
        ImGui::TextDisabled("Math");
        ImGui::Separator();
        addItem("Multiply",  NodeKind::Multiply);
        addItem("Add",       NodeKind::Add);
        addItem("Mix",       NodeKind::Mix);
        addItem("One Minus", NodeKind::OneMinus);
        addItem("Power",     NodeKind::Power);
        addItem("Clamp",     NodeKind::Clamp);

        ImGui::Spacing();
        ImGui::TextDisabled("Utilities");
        ImGui::Separator();
        addItem("Split RGB", NodeKind::SplitRGB);
        addItem("Make RGB",  NodeKind::MakeRGB);

        ImGui::EndPopup();
    }
}

void MaterialNodeEditor::RenderSidebar() {
    // Preview — render live with the compiled node shader so edits show up
    // immediately. Falls back to the material-based preview if not compiled.
    constexpr float kPrevSz = 220.0f;
    if (m_preview) {
        uint32_t tex = 0;
        if (m_compiledShader && m_compiled.valid) {
            std::vector<std::pair<std::string, std::string>> texBindings;
            for (auto& [nodeId, uname] : m_compiled.texUniforms) {
                GraphNode* node = m_graph.FindNode(nodeId);
                if (node && node->kind == NodeKind::Texture2D)
                    texBindings.emplace_back(uname, node->texPath);
            }
            tex = m_preview->RenderWithShader(m_compiledShader, texBindings);
        } else if (Material* mat = MaterialManager::GetOrLoad(m_matPath)) {
            tex = m_preview->Render(mat);
        }
        if (tex) ImGui::Image((ImTextureID)(intptr_t)tex,
                              ImVec2(kPrevSz, kPrevSz), ImVec2(0,1), ImVec2(1,0));
    }

    ImGui::Separator();
    ImGui::Spacing();

    // Compile status
    if (!m_compiled.valid && !m_compiled.error.empty()) {
        ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
        ImGui::TextColored(ImVec4(1.0f,0.4f,0.4f,1.0f), "Error: %s", m_compiled.error.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
    }

    // Selected node properties
    if (m_selectedNode >= 0) {
        GraphNode* node = m_graph.FindNode(m_selectedNode);
        if (node) RenderNodeProperties(*node);
    } else {
        ImGui::TextDisabled("Select a node to edit");
    }
}

void MaterialNodeEditor::RenderNodeProperties(GraphNode& node) {
    ImGui::Text("%s", node.Title());
    ImGui::Separator();
    ImGui::Spacing();

    bool changed = false;
    switch (node.kind) {
        case NodeKind::Color:
            ImGui::Text("Color");
            changed |= ImGui::ColorEdit3("##cprop", node.color, ImGuiColorEditFlags_PickerHueWheel);
            break;
        case NodeKind::Float:
            ImGui::Text("Value");
            changed |= ImGui::DragFloat("##fprop", &node.floatVal, 0.01f, 0.0f, 1.0f);
            break;
        case NodeKind::Texture2D: {
            ImGui::TextDisabled("Texture");
            ImGui::Spacing();
            if (!node.texPath.empty()) {
                // Preview thumbnail
                Texture* t = TextureManager::GetOrLoad(node.texPath);
                if (t) ImGui::Image((ImTextureID)(intptr_t)t->GetID(),
                                    ImVec2(120, 120), ImVec2(0,1), ImVec2(1,0));
                ImGui::TextDisabled("%s", fs::path(node.texPath).filename().string().c_str());
            } else {
                ImGui::TextDisabled("None");
            }
            ImGui::Spacing();
            if (ImGui::Button(ICON_FA_FOLDER_OPEN "  Browse##tx", ImVec2(-1, 0))) {
                std::string p = OpenTextureDlg();
                if (!p.empty()) { node.texPath = p; changed = true; }
            }
            if (!node.texPath.empty()) {
                if (ImGui::Button(ICON_FA_XMARK "  Clear##tx", ImVec2(-1, 0))) {
                    node.texPath.clear();
                    changed = true;
                }
            }
            break;
        }
        default:
            break;
    }

    // Default values for unconnected input pins
    if (!node.inputs.empty()) {
        ImGui::Spacing();
        ImGui::TextDisabled("Defaults (unconnected)");
        ImGui::Separator();
        for (auto& pin : node.inputs) {
            bool connected = m_graph.FindLinkToPin(pin.id) != nullptr;
            if (connected) continue;
            ImGui::PushID(pin.id);
            if (pin.type == PinType::Float)
                changed |= ImGui::DragFloat(pin.name.c_str(), &pin.defFloat, 0.01f, 0.0f, 1.0f);
            else if (pin.type == PinType::Vec3)
                changed |= ImGui::ColorEdit3(pin.name.c_str(), pin.defVec3);
            ImGui::PopID();
        }
    }

    if (changed) { m_dirty = true; m_compileTimer = 0.0f; }
}
