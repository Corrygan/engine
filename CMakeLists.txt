cmake_minimum_required(VERSION 3.20)
project(Engine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --------------------------------------------------------------------------
# Dear ImGui (docking) — уже лежит исходниками в imgui/, FetchContent не нужен.
# --------------------------------------------------------------------------
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/imgui)

# --------------------------------------------------------------------------
# Сам движок
# --------------------------------------------------------------------------
add_executable(Engine
    main.cpp
    UI/EditorUI.cpp

    Core/Guid.cpp
    Assets/MetaFile.cpp
    Assets/AssetDatabase.cpp
    Assets/SceneSerializer.cpp

    Renderer/Framebuffer.cpp
    Renderer/Shader.cpp
    Renderer/Mesh.cpp
    Renderer/Grid.cpp
    Renderer/EditorCamera.cpp
    Renderer/Picking.cpp
    Renderer/SceneRenderer.cpp

    glad/src/gl.c

    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(Engine PRIVATE
    .
    UI
    glad/include
    glm
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${CMAKE_SOURCE_DIR}/lib/include/GLFW   # чтобы #include "glfw3.h" работал без префикса GLFW/
)

find_package(OpenGL REQUIRED)

# GLFW уже собран и лежит как готовый .lib в lib/lib/ — просто линкуемся
# напрямую на файл, отдельный target тут заводить не нужно.
target_link_libraries(Engine PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/lib/glfw3.lib
    OpenGL::GL
)

if (MSVC)
    target_compile_definitions(Engine PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Чтобы при запуске из Visual Studio рабочая директория была корнем проекта,
# а не папкой со сгенерированными build-файлами — важно для будущей загрузки
# ассетов по относительным путям.
set_target_properties(Engine PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
