cmake_minimum_required(VERSION 3.20)

set(VCPKG_TARGET_TRIPLET "x64-windows-static-md" CACHE STRING "")
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

project(Engine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/imgui)

add_executable(Engine
    main.cpp
    UI/EditorUI.cpp
    UI/MaterialNodeEditor.cpp

    Core/Guid.cpp
    Assets/MetaFile.cpp
    Assets/AssetDatabase.cpp
    Assets/SceneSerializer.cpp
    Assets/ModelImporter.cpp

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

    imnodes/imnodes.cpp

    glad/src/gl.c

    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    ${IMGUI_DIR}/ImGuizmo.cpp
)

target_include_directories(Engine PRIVATE
    .
    UI
    glad/include
    glm
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${CMAKE_SOURCE_DIR}/lib/include/GLFW
    ${CMAKE_SOURCE_DIR}/imnodes
)

find_package(OpenGL REQUIRED)
find_package(assimp CONFIG REQUIRED)

target_link_libraries(Engine PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/lib/glfw3.lib
    OpenGL::GL
    assimp::assimp
    Comdlg32
    Shell32
)

if (MSVC)
    target_compile_definitions(Engine PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

target_compile_definitions(Engine PRIVATE
    ENGINE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

set_target_properties(Engine PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
