#include "SceneRenderer.h"
#include "Framebuffer.h"
#include "Shader.h"
#include "Mesh.h"
#include "Grid.h"
#include "../Scene/Transform.h"
#include "glad/gl.h"
#include <glm/glm.hpp>

namespace {
    // --- Обычный лит-шейдер для объектов сцены ---
    const char* kVertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vNormal;

void main() {
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
}
)";

    const char* kFragmentShaderSrc = R"(
#version 330 core
in vec3 vNormal;

uniform vec3 uColor;

out vec4 FragColor;

void main() {
    vec3 normal = normalize(vNormal);
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 ambient = 0.3 * uColor;
    vec3 diffuse = diff * uColor;
    FragColor = vec4(ambient + diffuse, 1.0);
}
)";

    // --- Безсветовой цветной шейдер для сетки (цвет приходит из вершины) ---
    const char* kLineVertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;

uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vColor;

void main() {
    vColor = aColor;
    gl_Position = uProjection * uView * vec4(aPos, 1.0);
}
)";

    const char* kLineFragmentShaderSrc = R"(
#version 330 core
in vec3 vColor;
out vec4 FragColor;

void main() {
    FragColor = vec4(vColor, 1.0);
}
)";

    // --- Шейдер контура выделения: раздувает вершину вдоль её мировой нормали ---
    const char* kOutlineVertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform float uOutlineWidth;

void main() {
    vec3 worldNormal = normalize(mat3(transpose(inverse(uModel))) * aNormal);
    vec3 worldPos = vec3(uModel * vec4(aPos, 1.0)) + worldNormal * uOutlineWidth;
    gl_Position = uProjection * uView * vec4(worldPos, 1.0);
}
)";

    const char* kOutlineFragmentShaderSrc = R"(
#version 330 core
uniform vec3 uOutlineColor;
out vec4 FragColor;

void main() {
    FragColor = vec4(uOutlineColor, 1.0);
}
)";
}

SceneRenderer::SceneRenderer() {
    m_framebuffer = new Framebuffer(64, 64);  // реальный размер выставится при первом Render()
    m_shader = new Shader(kVertexShaderSrc, kFragmentShaderSrc);
    m_lineShader = new Shader(kLineVertexShaderSrc, kLineFragmentShaderSrc);
    m_outlineShader = new Shader(kOutlineVertexShaderSrc, kOutlineFragmentShaderSrc);
    m_cube = new CubeMesh();
    m_grid = new Grid();
}

SceneRenderer::~SceneRenderer() {
    delete m_grid;
    delete m_cube;
    delete m_outlineShader;
    delete m_lineShader;
    delete m_shader;
    delete m_framebuffer;
}

uint32_t SceneRenderer::Render(const std::vector<GameObject>& objects, int selectedIndex,
    int width, int height,
    const glm::mat4& view, const glm::mat4& projection) {
    if (width <= 0 || height <= 0) return 0;

    m_framebuffer->Resize(width, height);
    m_framebuffer->Bind();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_STENCIL_TEST);
    glStencilMask(0xFF);
    glClearColor(0.10f, 0.11f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // --- Сетка: рисуется как обычная геометрия, в стенсил не пишет ---
    glStencilFunc(GL_ALWAYS, 0, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    m_lineShader->Bind();
    m_lineShader->SetMat4("uView", view);
    m_lineShader->SetMat4("uProjection", projection);
    m_grid->Draw();
    m_lineShader->Unbind();

    // --- Объекты сцены: у всех одинаковый цвет, выделение теперь через
    // контур, а не перекраску всего объекта целиком ---
    m_shader->Bind();
    m_shader->SetMat4("uView", view);
    m_shader->SetMat4("uProjection", projection);
    m_shader->SetVec3("uColor", glm::vec3(0.55f, 0.58f, 0.65f));

    for (size_t i = 0; i < objects.size(); ++i) {
        glm::mat4 model = BuildModelMatrix(objects[i]);
        m_shader->SetMat4("uModel", model);

        // Силуэт выделенного объекта помечаем в стенсил-буфере значением 1 —
        // он понадобится сразу после, чтобы нарисовать контур только по краю.
        bool isSelected = (static_cast<int>(i) == selectedIndex);
        if (isSelected) {
            glStencilFunc(GL_ALWAYS, 1, 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        }
        else {
            glStencilFunc(GL_ALWAYS, 0, 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        }

        m_cube->Draw();
    }

    // --- Контур: та же геометрия выделенного объекта, чуть раздутая вдоль
    // нормалей, рисуется только там, где стенсил НЕ равен 1 — то есть только
    // тонкое "кольцо" вокруг исходного силуэта, а не сам объект целиком ---
    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(objects.size())) {
        glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glStencilMask(0x00);

        m_outlineShader->Bind();
        glm::mat4 model = BuildModelMatrix(objects[selectedIndex]);
        m_outlineShader->SetMat4("uModel", model);
        m_outlineShader->SetMat4("uView", view);
        m_outlineShader->SetMat4("uProjection", projection);
        m_outlineShader->SetFloat("uOutlineWidth", 0.03f);
        m_outlineShader->SetVec3("uOutlineColor", glm::vec3(0.95f, 0.65f, 0.20f));

        m_cube->Draw();

        m_outlineShader->Unbind();
        glStencilMask(0xFF);
    }

    glDisable(GL_STENCIL_TEST);
    m_framebuffer->Unbind();

    return m_framebuffer->GetColorTexture();
}
