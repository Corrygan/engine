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
#include <unordered_set>
#include <unordered_map>
#include <cstdint>
#include "../Scene/Scene.h"
#include "../Renderer/EditorCamera.h"

struct AssetItem {
    std::string path;
    std::string name;
    std::string ext;
    bool        isDir = false;
};

class SceneRenderer;
class LuaScripting;

struct ConsoleMessage {
    enum Type { Info, Warning, Error, Success };
    Type type;
    std::string text;
    std::string timestamp;
};

enum class PlayState { Editing, Playing, Paused };

// One undo/redo entry: a deep clone of the scene plus the selected entity
// (expressed in the snapshot's own id space).
struct SceneSnapshot {
    Scene        scene;
    entt::entity selected = entt::null;
};

// An open scene "document". Several can be open at once; you switch between them
// like browser tabs and only the active one is shown/edited. Each keeps its own
// scene + selection + undo history. The ACTIVE document's state lives live in the
// editor's m_scene/m_selected/undo members; the fields below stash an INACTIVE
// document's state until it's switched back in.
struct SceneDoc {
    uint32_t    id    = 0;
    std::string name  = "Untitled";
    std::string path;                 // .escn on disk; empty if never saved
    bool        dirty = false;

    Scene                      scene;             // stashed registry (inactive only)
    entt::entity               selected = entt::null;
    std::vector<SceneSnapshot> undoStack;
    std::vector<SceneSnapshot> redoStack;
    SceneSnapshot              baseline;
    uint64_t                   sceneRevision    = 0;
    uint64_t                   baselineRevision = 0;
};

// A project: the top-level container (a .fcproj file + a folder of assets).
// Scenes and other assets live inside it; the asset browser is rooted here.
struct Project {
    std::string file;               // absolute path to the .fcproj
    std::string dir;                // project root directory
    std::string name;               // display name
    std::string assetSub = "Assets";// asset subfolder (relative to dir)
    std::string assetDir;           // absolute asset folder (dir/assetSub)
    std::string startupScene;       // scene opened on load (relative to dir, optional)
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
    void SelectEntity(entt::entity e);

    bool LoadProject(const std::string& fcprojPath);   // called by the launcher + File menu

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
    void RenderViewportToolbar();    // icon-only bar above the viewport (play/render/settings)
    void RenderViewportTimeline();   // placeholder timeline strip below the viewport
    void RenderSceneTabs();          // browser-style scene tabs at the top of the viewport

    // Play mode
    void EnterPlayMode();
    void ExitPlayMode();
    void TogglePause();
    void UpdatePlayMode();           // per-frame game update while playing
    void RenderAssetBrowser();
    void RenderEntityNode(entt::entity e);
    void ReparentEntity(entt::entity child, entt::entity newParent);  // null = make root
    void DestroyEntitySubtree(entt::entity e);                        // entity + descendants
    entt::entity DuplicateEntity(entt::entity src);
    void ImportModelAsync(const std::string& sourcePath);   // assimp on a worker, place on main
    void CreatePrefab(entt::entity e);                      // save an entity as a .fcprefab
    void InstantiatePrefab(const std::string& path);        // stamp a .fcprefab into the scene

    static constexpr float kViewportToolbarH  = 38.0f;
    static constexpr float kViewportTimelineH = 96.0f;
    void RenderTransformEditor(entt::entity e);
    void RenderUnsavedChangesDialog();
    void RenderCloseSceneDialog();             // per-scene close confirmation
    void RequestCloseDocument(uint32_t id);    // prompt if dirty, else close

    // Projects (in-editor switching; the launcher window opens the first one).
    bool CreateProject(const std::string& fcprojPath, const std::string& name);
    void RequestClose();   // X/Exit: prompt if dirty, else close
    void AddConsoleMessage(const std::string& message, ConsoleMessage::Type type);
    bool SaveCurrentScene();

    // Multi-scene documents (tab switching; one active/visible at a time).
    SceneDoc* FindDoc(uint32_t id);
    SceneDoc& ActiveDoc();
    uint32_t  NewDocument(const std::string& name = "Untitled");
    void      SwitchTo(uint32_t id);             // stash active state, load target's
    bool      SaveDocument(uint32_t id, bool saveAs = false);
    void      CloseDocument(uint32_t id);
    void      OpenScene(const std::string& path);
    bool      AnyDocDirty() const;

    // Undo/redo — snapshot transactions with per-gesture coalescing.
    void          MarkDirty();             // registry changed: set dirty + bump revision
    void          Undo();
    void          Redo();
    void          ClearUndoHistory();
    void          UpdateUndoCoalescing();  // once per frame: commit a step when a gesture ends
    void          CommitEdit();            // push the baseline as an undo step, refresh baseline
    SceneSnapshot CaptureSnapshot();
    void          RestoreSnapshot(const SceneSnapshot& snap);

private:
    GLFWwindow* m_window = nullptr;
    SceneRenderer* m_sceneRenderer = nullptr;
    EditorCamera m_camera;
    float m_viewportDragAccum = 0.0f;
    entt::entity m_selected = entt::null;
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
    std::unordered_set<std::string> m_compiledGraphs;   // node materials already precompiled

    bool m_sceneDirty = false;            // any open document has unsaved changes
    bool m_showUnsavedChangesDialog = false;

    // The ECS registry is the editor's single source of truth.
    Scene                       m_scene;

    // Open scene documents (browser-style tabs; one active at a time). The active
    // doc's scene/selection/undo are live in m_scene/m_selected/undo members; the
    // others are stashed in their SceneDoc until switched back in.
    std::vector<SceneDoc>       m_docs;
    uint32_t                    m_activeDoc   = 0;
    uint32_t                    m_nextDocId   = 1;
    uint32_t                    m_tabSelectId   = 0;   // request the tab bar to select this doc
    uint32_t                    m_closePromptDoc = 0;  // doc awaiting close confirmation

    // Project (the currently-open project; set by LoadProject)
    Project                     m_project;

    // Play mode
    PlayState               m_playState = PlayState::Editing;
    Scene                   m_playBackup;          // full snapshot, restored on Stop
    std::unordered_map<entt::entity, entt::entity> m_playMap;   // live entity → backup entity
    float                   m_playTime = 0.0f;

    // Undo/redo
    std::vector<SceneSnapshot> m_undoStack;
    std::vector<SceneSnapshot> m_redoStack;
    SceneSnapshot              m_baseline;             // last settled state (pre-edit reference)
    uint64_t                   m_sceneRevision    = 0; // bumped by MarkDirty()
    uint64_t                   m_baselineRevision = 0;
    static constexpr size_t    kMaxUndo           = 64;

    MaterialPreviewRenderer* m_previewRenderer = nullptr;
    MaterialNodeEditor*      m_nodeEditor      = nullptr;
    LuaScripting*            m_lua             = nullptr;

public:
    bool m_pendingMinimize = false;
    bool m_pendingMaximize = false;
};
