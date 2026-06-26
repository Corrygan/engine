cmake_minimum_required(VERSION 3.20)

set(VCPKG_TARGET_TRIPLET "x64-windows-static-md" CACHE STRING "")
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

project(Engine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Third-party packages ────────────────────────────────────────────────────
find_package(OpenGL REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(Lua REQUIRED)
find_package(sol2 CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/imgui)

if (MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# ── Third-party static libraries ────────────────────────────────────────────
# glad — OpenGL function loader.
add_library(glad STATIC glad/src/gl.c)
target_include_directories(glad PUBLIC ${CMAKE_SOURCE_DIR}/glad/include)

# Dear ImGui + GLFW/OpenGL3 backends + ImGuizmo.
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    ${IMGUI_DIR}/ImGuizmo.cpp
)
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${CMAKE_SOURCE_DIR}/lib/include/GLFW          # imgui_impl_glfw.cpp -> <glfw3.h>
)

# imnodes — node canvas (used by the material node editor).
add_library(imnodes STATIC imnodes/imnodes.cpp)
target_include_directories(imnodes PUBLIC ${CMAKE_SOURCE_DIR}/imnodes)
target_link_libraries(imnodes PUBLIC imgui)

# Shared header-only conveniences for the engine libs: the source root (so both
# "../X/Y.h" and "X/Y.h" includes resolve) and GLM.
add_library(engine_common INTERFACE)
target_include_directories(engine_common INTERFACE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/glm
)

# ── Engine libraries (layered: each depends only on the ones above it) ───────
# Core: ids, math/util helpers. No OpenGL, no ImGui.
add_library(EngineCore STATIC
    Core/Guid.cpp
)
target_link_libraries(EngineCore PUBLIC engine_common)

# Scene: the EnTT registry + components.
add_library(EngineScene STATIC
    Scene/Scene.cpp
)
target_link_libraries(EngineScene PUBLIC EngineCore EnTT::EnTT)

# Renderer: GL rendering, meshes, materials, post-processing.
add_library(EngineRenderer STATIC
    Renderer/Framebuffer.cpp
    Renderer/Shader.cpp
    Renderer/Mesh.cpp
    Renderer/ModelMesh.cpp
    Renderer/Material.cpp
    Renderer/MaterialManager.cpp
    Renderer/MaterialPreviewRenderer.cpp
    Renderer/Texture.cpp
    Renderer/TextureManager.cpp
    Renderer/Grid.cpp
    Renderer/EditorCamera.cpp
    Renderer/Picking.cpp
    Renderer/SceneRenderer.cpp
    Renderer/MaterialGraph.cpp
    Renderer/NodeCompiler.cpp
    Renderer/EnvironmentMap.cpp
)
target_link_libraries(EngineRenderer PUBLIC EngineScene glad OpenGL::GL)

# Assets: import + serialization (asset database, meta files, scenes, models).
add_library(EngineAssets STATIC
    Assets/MetaFile.cpp
    Assets/AssetDatabase.cpp
    Assets/SceneSerializer.cpp
    Assets/ModelImporter.cpp
    Assets/ProjectFile.cpp
)
target_link_libraries(EngineAssets PUBLIC EngineScene PRIVATE assimp::assimp)

# Scripting: Lua runtime (sol2 hidden behind a pimpl).
add_library(EngineScripting STATIC
    Scripting/LuaScripting.cpp
)
target_link_libraries(EngineScripting PUBLIC EngineScene PRIVATE ${LUA_LIBRARIES} sol2::sol2)
target_include_directories(EngineScripting PRIVATE ${LUA_INCLUDE_DIR})

# Jobs: CPU worker-thread pool + cooperative wait + main-thread marshaling.
# Foundational (std threads only); no engine deps beyond the shared includes.
add_library(EngineJobs STATIC
    Jobs/JobSystem.cpp
)
target_link_libraries(EngineJobs PUBLIC engine_common)
find_package(Threads REQUIRED)
target_link_libraries(EngineJobs PUBLIC Threads::Threads)

# ── Editor application ───────────────────────────────────────────────────────
add_executable(Engine
    main.cpp
    UI/EditorUI.cpp
    UI/MaterialNodeEditor.cpp
)
target_include_directories(Engine PRIVATE ${CMAKE_SOURCE_DIR}/UI)
target_link_libraries(Engine PRIVATE
    EngineCore
    EngineScene
    EngineRenderer
    EngineAssets
    EngineScripting
    EngineJobs
    imgui
    imnodes
    glad
    ${CMAKE_SOURCE_DIR}/lib/lib/glfw3.lib
    OpenGL::GL
    Comdlg32
    Shell32
    Dwmapi
)
target_compile_definitions(Engine PRIVATE ENGINE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
set_target_properties(Engine PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
