#include "SceneRenderer.h"
#include "Framebuffer.h"
#include "Shader.h"
#include "Mesh.h"
#include "ModelMesh.h"
#include "MaterialManager.h"
#include "TextureManager.h"
#include "Grid.h"
#include "../Scene/Transform.h"
#include "glad/gl.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>

namespace {
    const char* kVertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in vec3 aBitangent;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;
out vec3 vTangent;
out vec3 vBitangent;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos  = worldPos.xyz;
    mat3 nm    = mat3(transpose(inverse(uModel)));
    vNormal    = nm * aNormal;
    vTangent   = nm * aTangent;
    vBitangent = nm * aBitangent;
    vUV        = aUV;
    gl_Position = uProjection * uView * worldPos;
}
)";

    const char* kFragmentShaderSrc = R"(
#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;
in vec3 vTangent;
in vec3 vBitangent;

uniform vec3      uCamPos;
uniform vec3      uColor;
uniform float     uMetallic;
uniform float     uRoughness;
uniform vec3      uEmissive;

uniform sampler2D uAlbedoTex;
uniform sampler2D uNormalTex;
uniform sampler2D uMetalRoughTex;
uniform int       uHasAlbedoTex;
uniform int       uHasNormalTex;
uniform int       uHasMetalRoughTex;
uniform int       uHasTangents;

out vec4 FragColor;

const float PI = 3.14159265359;

float D_GGX(float NdotH, float r) {
    float a  = r * r;
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

float G_Smith(float NdotV, float NdotL, float r) {
    float k  = (r + 1.0) * (r + 1.0) / 8.0;
    float gv = NdotV / (NdotV * (1.0 - k) + k);
    float gl = NdotL / (NdotL * (1.0 - k) + k);
    return gv * gl;
}

vec3 F_Schlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec3  albedo    = (uHasAlbedoTex != 0)
                      ? pow(texture(uAlbedoTex, vUV).rgb, vec3(2.2))
                      : pow(uColor, vec3(2.2));
    float metallic  = uMetallic;
    float roughness = max(uRoughness, 0.04);

    float ao = 1.0;
    if (uHasMetalRoughTex != 0) {
        vec3 orm  = texture(uMetalRoughTex, vUV).rgb;
        ao        = orm.r;
        roughness = max(orm.g, 0.04);
        metallic  = orm.b;
    }

    vec3 N;
    if (uHasTangents != 0 && uHasNormalTex != 0) {
        vec3 ns  = texture(uNormalTex, vUV).rgb * 2.0 - 1.0;
        mat3 TBN = mat3(normalize(vTangent), normalize(vBitangent), normalize(vNormal));
        N = normalize(TBN * ns);
    } else {
        N = normalize(vNormal);
    }
    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 L = normalize(vec3(1.2, 2.0, 0.8));
    vec3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);

    vec3 F0  = mix(vec3(0.04), albedo, metallic);
    vec3 F   = F_Schlick(max(dot(H, V), 0.0), F0);
    float D  = D_GGX(NdotH, roughness);
    float G  = G_Smith(NdotV, NdotL, roughness);

    vec3 spec    = (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);
    vec3 kD      = (vec3(1.0) - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / PI;

    vec3 lightColor = vec3(3.2, 3.0, 2.8);
    vec3 Lo         = (diffuse + spec) * lightColor * NdotL;
    vec3 ambient    = vec3(0.04) * albedo * ao;
    vec3 color      = ambient + Lo + uEmissive;

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
)";

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

    const char* kOutlineVertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform float uOutlineWidth;

void main() {
    vec3 worldPos = vec3(uModel * vec4(aPos, 1.0));
    vec3 dir = normalize(worldPos - vec3(uModel[3]));
    worldPos += dir * uOutlineWidth;
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
    m_framebuffer = new Framebuffer(64, 64);
    m_shader = new Shader(kVertexShaderSrc, kFragmentShaderSrc);
    m_lineShader = new Shader(kLineVertexShaderSrc, kLineFragmentShaderSrc);
    m_outlineShader = new Shader(kOutlineVertexShaderSrc, kOutlineFragmentShaderSrc);
    m_cube = new CubeMesh();
    m_sphere = new SphereMesh();
    m_plane = new PlaneMesh();
    m_grid = new Grid();
}

void SceneRenderer::RegisterNodeShader(const std::string& matPath,
                                       Shader* shader,
                                       std::vector<std::pair<std::string,std::string>> texBindings) {
    // SceneRenderer takes ownership. Delete any previously-registered shader.
    auto it = m_nodeShaders.find(matPath);
    if (it != m_nodeShaders.end()) {
        delete it->second.shader;
    }
    m_nodeShaders[matPath] = { shader, std::move(texBindings) };
}

void SceneRenderer::UnregisterNodeShader(const std::string& matPath) {
    auto it = m_nodeShaders.find(matPath);
    if (it != m_nodeShaders.end()) {
        delete it->second.shader;
        m_nodeShaders.erase(it);
    }
}

SceneRenderer::~SceneRenderer() {
    for (auto& [path, mesh]  : m_modelCache)  delete mesh;
    for (auto& [path, entry] : m_nodeShaders) delete entry.shader;
    delete m_grid;
    delete m_plane;
    delete m_sphere;
    delete m_cube;
    delete m_outlineShader;
    delete m_lineShader;
    delete m_shader;
    delete m_framebuffer;
}

ModelMesh* SceneRenderer::GetOrLoadModel(const std::string& emdlPath) {
    if (emdlPath.empty()) return nullptr;
    auto it = m_modelCache.find(emdlPath);
    if (it != m_modelCache.end()) return it->second;
    ModelMesh* mesh = ModelMesh::LoadFromEmdl(emdlPath);
    m_modelCache[emdlPath] = mesh;
    return mesh;
}

PrimitiveMesh* SceneRenderer::GetMeshForType(PrimitiveType type) const {
    switch (type) {
    case PrimitiveType::Cube:   return m_cube;
    case PrimitiveType::Sphere: return m_sphere;
    case PrimitiveType::Plane:  return m_plane;
    case PrimitiveType::Camera:
    case PrimitiveType::Light:
    case PrimitiveType::Empty:
    default:
        return nullptr;
    }
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

    glStencilFunc(GL_ALWAYS, 0, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    m_lineShader->Bind();
    m_lineShader->SetMat4("uView", view);
    m_lineShader->SetMat4("uProjection", projection);
    m_grid->Draw();
    m_lineShader->Unbind();

    static const glm::vec3 kDefaultColor(0.55f, 0.58f, 0.65f);

    // Extract camera world position from inverse view matrix
    glm::mat4 invView = glm::affineInverse(view);
    glm::vec3 camPos(invView[3]);

    m_shader->Bind();
    m_shader->SetMat4("uView",       view);
    m_shader->SetMat4("uProjection", projection);
    m_shader->SetVec3("uCamPos",     camPos);
    m_shader->SetInt("uAlbedoTex",     0);
    m_shader->SetInt("uNormalTex",     1);
    m_shader->SetInt("uMetalRoughTex", 2);

    for (size_t i = 0; i < objects.size(); ++i) {
        const GameObject& obj = objects[i];

        // Resolve mesh first so hasTangents is known before setting uniforms
        PrimitiveMesh* primMesh  = nullptr;
        ModelMesh*     modelMesh = nullptr;
        if (obj.type == PrimitiveType::Model)
            modelMesh = GetOrLoadModel(obj.modelPath);
        else
            primMesh = GetMeshForType(obj.type);

        if (!primMesh && !modelMesh) continue;

        glm::vec3 color(kDefaultColor);
        float     metallic  = 0.0f;
        float     roughness = 0.5f;
        glm::vec3 emissive(0.0f);
        Texture*  albedoTex = nullptr;
        Texture*  normalTex = nullptr;
        Texture*  mrTex     = nullptr;

        if (!obj.materialPath.empty()) {
            Material* mat = MaterialManager::GetOrLoad(obj.materialPath);
            if (mat) {
                color     = glm::vec3(mat->color[0], mat->color[1], mat->color[2]);
                metallic  = mat->metallic;
                roughness = mat->roughness;
                emissive  = glm::vec3(mat->emissiveColor[0], mat->emissiveColor[1], mat->emissiveColor[2])
                            * mat->emissiveIntensity;
                if (!mat->albedoTexture.empty())
                    albedoTex = TextureManager::GetOrLoad(mat->albedoTexture);
                if (!mat->normalTexture.empty())
                    normalTex = TextureManager::GetOrLoad(mat->normalTexture);
                if (!mat->ormTexture.empty())
                    mrTex = TextureManager::GetOrLoad(mat->ormTexture);
            }
        }

        const bool hasTangents = (modelMesh != nullptr);
        glm::mat4 model = BuildModelMatrix(obj);
        bool isSelected = (static_cast<int>(i) == selectedIndex);

        // Stencil setup (same for both shader paths)
        if (isSelected) {
            glStencilFunc(GL_ALWAYS, 1, 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        } else {
            glStencilFunc(GL_ALWAYS, 0, 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        }

        // ── Node-compiled shader? ─────────────────────────────────────────
        auto nodeIt = !obj.materialPath.empty()
                    ? m_nodeShaders.find(obj.materialPath)
                    : m_nodeShaders.end();

        if (nodeIt != m_nodeShaders.end() && nodeIt->second.shader) {
            Shader* cs = nodeIt->second.shader;
            cs->Bind();
            cs->SetMat4("uModel", model);
            cs->SetMat4("uView",  view);
            cs->SetMat4("uProj",  projection);
            cs->SetVec3("uCamPos", camPos);

            int unit = 0;
            for (auto& [uname, texPath] : nodeIt->second.texBindings) {
                Texture* tex = texPath.empty() ? nullptr : TextureManager::GetOrLoad(texPath);
                glActiveTexture(GL_TEXTURE0 + unit);
                glBindTexture(GL_TEXTURE_2D, tex ? tex->GetID() : 0);
                glUniform1i(glGetUniformLocation(cs->GetID(), uname.c_str()), unit);
                ++unit;
            }

            if (modelMesh) modelMesh->Draw();
            else           primMesh->Draw();

            cs->Unbind();

            // Restore default shader state for remaining objects
            m_shader->Bind();
            m_shader->SetMat4("uView",       view);
            m_shader->SetMat4("uProjection", projection);
            m_shader->SetVec3("uCamPos",     camPos);
        } else {
            // ── Default PBR shader ────────────────────────────────────────
            m_shader->SetVec3 ("uColor",            color);
            m_shader->SetFloat("uMetallic",          metallic);
            m_shader->SetFloat("uRoughness",         roughness);
            m_shader->SetVec3 ("uEmissive",          emissive);
            m_shader->SetInt  ("uHasAlbedoTex",      albedoTex ? 1 : 0);
            m_shader->SetInt  ("uHasNormalTex",       normalTex ? 1 : 0);
            m_shader->SetInt  ("uHasMetalRoughTex",  mrTex     ? 1 : 0);
            m_shader->SetInt  ("uHasTangents",        hasTangents ? 1 : 0);

            if (albedoTex) albedoTex->Bind(0);
            else { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, 0); }
            if (normalTex) normalTex->Bind(1);
            else { glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, 0); }
            if (mrTex) mrTex->Bind(2);
            else { glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, 0); }

            m_shader->SetMat4("uModel", model);

            if (modelMesh) modelMesh->Draw();
            else           primMesh->Draw();
        }
    }

    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(objects.size())) {
        const GameObject& sel = objects[selectedIndex];

        PrimitiveMesh* primMesh  = nullptr;
        ModelMesh*     modelMesh = nullptr;
        if (sel.type == PrimitiveType::Model)
            modelMesh = GetOrLoadModel(sel.modelPath);
        else
            primMesh = GetMeshForType(sel.type);

        if (primMesh || modelMesh) {
            glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
            glStencilMask(0x00);

            m_outlineShader->Bind();
            glm::mat4 model = BuildModelMatrix(sel);
            m_outlineShader->SetMat4("uModel", model);
            m_outlineShader->SetMat4("uView", view);
            m_outlineShader->SetMat4("uProjection", projection);
            m_outlineShader->SetFloat("uOutlineWidth", 0.03f);
            m_outlineShader->SetVec3("uOutlineColor", glm::vec3(0.95f, 0.65f, 0.20f));

            if (modelMesh) modelMesh->Draw();
            else           primMesh->Draw();

            m_outlineShader->Unbind();
            glStencilMask(0xFF);
        }
    }

    glDisable(GL_STENCIL_TEST);
    m_framebuffer->Unbind();

    return m_framebuffer->GetColorTexture();
}