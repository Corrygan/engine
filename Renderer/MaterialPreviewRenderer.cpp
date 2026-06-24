#include "MaterialPreviewRenderer.h"
#include "Framebuffer.h"
#include "Shader.h"
#include "Mesh.h"
#include "TextureManager.h"
#include "glad/gl.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace {
    const char* kPreviewVert = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 uMVP;
uniform mat4 uModel;

out vec3 vNormal;
out vec3 vWorldPos;
out vec3 vLocalNormal;

void main() {
    vec4 wp      = uModel * vec4(aPos, 1.0);
    vWorldPos    = wp.xyz;
    vNormal      = mat3(transpose(inverse(uModel))) * aNormal;
    vLocalNormal = aNormal;
    gl_Position  = uMVP * vec4(aPos, 1.0);
}
)";

    const char* kPreviewFrag = R"(
#version 330 core
in vec3 vNormal;
in vec3 vWorldPos;
in vec3 vLocalNormal;

uniform vec3      uColor;
uniform float     uMetallic;
uniform float     uRoughness;
uniform vec3      uEmissive;
uniform vec3      uCamPos;
uniform sampler2D uAlbedoTex;
uniform bool      uHasAlbedo;

out vec4 FragColor;

const float PI = 3.14159265359;

vec2 sphereUV(vec3 n) {
    float u = atan(n.z, n.x) / (2.0 * PI) + 0.5;
    float v = acos(clamp(n.y, -1.0, 1.0)) / PI;
    return vec2(u, v);
}

float D_GGX(float NdotH, float r) {
    float a = r*r; float a2 = a*a;
    float d = NdotH*NdotH*(a2-1.0)+1.0;
    return a2/(PI*d*d);
}
float G_Smith(float NdotV, float NdotL, float r) {
    float k = (r+1.0)*(r+1.0)/8.0;
    return (NdotV/(NdotV*(1.0-k)+k))*(NdotL/(NdotL*(1.0-k)+k));
}
vec3 F_Schlick(float c, vec3 F0) {
    return F0+(1.0-F0)*pow(clamp(1.0-c,0.0,1.0),5.0);
}

void main() {
    vec2 uv = sphereUV(normalize(vLocalNormal));

    vec3 albedo;
    if (uHasAlbedo) {
        vec4 texSample = texture(uAlbedoTex, uv);
        albedo = pow(texSample.rgb, vec3(2.2));
    } else {
        albedo = pow(uColor, vec3(2.2));
    }

    float metallic = uMetallic;
    float rough    = max(uRoughness, 0.04);

    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCamPos - vWorldPos);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 Lo = vec3(0.0);

    vec3 lights[3];
    lights[0] = normalize(vec3( 1.2, 2.0,  1.5));
    lights[1] = normalize(vec3(-1.5, 0.5,  1.0));
    lights[2] = normalize(vec3( 0.0,-1.0,  2.0));
    vec3 lightColors[3];
    lightColors[0] = vec3(3.2, 3.0, 2.8);
    lightColors[1] = vec3(0.6, 0.8, 1.2);
    lightColors[2] = vec3(1.0, 1.0, 1.0);

    for (int i = 0; i < 3; ++i) {
        vec3 L = lights[i];
        vec3 H = normalize(V+L);
        float NdotL = max(dot(N,L),0.0);
        float NdotV = max(dot(N,V),0.0);
        float NdotH = max(dot(N,H),0.0);
        float D = D_GGX(NdotH, rough);
        float G = G_Smith(NdotV, NdotL, rough);
        vec3  F = F_Schlick(max(dot(H,V),0.0), F0);
        vec3 spec = (D*G*F)/max(4.0*NdotV*NdotL,0.001);
        vec3 kD   = (vec3(1.0)-F)*(1.0-metallic);
        Lo += (kD*albedo/PI+spec)*lightColors[i]*NdotL;
    }

    vec3 ambient = vec3(0.05)*albedo;
    vec3 color   = ambient + Lo + uEmissive;
    color = color/(color+vec3(1.0));
    color = pow(color, vec3(1.0/2.2));
    FragColor = vec4(color, 1.0);
}
)";
}

MaterialPreviewRenderer::MaterialPreviewRenderer() {
    m_fb     = new Framebuffer(188, 188);
    m_shader = new Shader(kPreviewVert, kPreviewFrag);
    m_sphere = new SphereMesh(32, 48);
}

MaterialPreviewRenderer::~MaterialPreviewRenderer() {
    ClearCache();
    delete m_sphere;
    delete m_shader;
    delete m_fb;
}

uint32_t MaterialPreviewRenderer::CopyToTexture(uint32_t srcTex) {
    constexpr int W = 188, H = 188;
    uint32_t dst;
    glGenTextures(1, &dst);
    glBindTexture(GL_TEXTURE_2D, dst);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    uint32_t readFBO;
    glGenFramebuffers(1, &readFBO);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, readFBO);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, srcTex, 0);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, W, H);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &readFBO);
    glBindTexture(GL_TEXTURE_2D, 0);
    return dst;
}

uint32_t MaterialPreviewRenderer::GetPreview(Material* mat, const std::string& path) {
    auto it = m_cache.find(path);
    if (it != m_cache.end()) return it->second;

    uint32_t rendered = Render(mat);
    if (!rendered) return 0;

    uint32_t cached = CopyToTexture(rendered);
    m_cache[path] = cached;
    return cached;
}

void MaterialPreviewRenderer::InvalidatePreview(const std::string& path) {
    auto it = m_cache.find(path);
    if (it != m_cache.end()) {
        glDeleteTextures(1, &it->second);
        m_cache.erase(it);
    }
}

void MaterialPreviewRenderer::ClearCache() {
    for (auto& [p, tex] : m_cache)
        glDeleteTextures(1, &tex);
    m_cache.clear();
}

uint32_t MaterialPreviewRenderer::RenderWithShader(
        Shader* shader,
        const std::vector<std::pair<std::string, std::string>>& texBindings) {
    if (!shader) return 0;

    m_fb->Bind();
    glViewport(0, 0, 188, 188);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.12f, 0.12f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 model      = glm::mat4(1.0f);
    glm::vec3 camPos     = glm::vec3(0.0f, 0.0f, 2.5f);
    glm::mat4 view       = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 projection = glm::perspective(glm::radians(40.0f), 1.0f, 0.1f, 10.0f);

    shader->Bind();
    GLuint pid = shader->GetID();
    glUniformMatrix4fv(glGetUniformLocation(pid, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(pid, "uView"),  1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(pid, "uProj"),  1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(glGetUniformLocation(pid, "uCamPos"),      1, glm::value_ptr(camPos));

    // The node program is shared with the scene; force neutral preview lighting
    // (fixed key light, no scene lights, no shadow sampling). Shadow samplers
    // are pointed at dedicated units so the cube sampler never collides with a
    // material sampler2D on unit 0.
    glUniform1i(glGetUniformLocation(pid, "uNumLights"),        0);
    glUniform1i(glGetUniformLocation(pid, "uShadowLight"),      -1);
    glUniform1i(glGetUniformLocation(pid, "uPointShadowLight"), -1);
    glUniform1i(glGetUniformLocation(pid, "uShadowMap"),         7);
    glUniform1i(glGetUniformLocation(pid, "uPointShadow"),       6);
    glUniform1f(glGetUniformLocation(pid, "uTime"),             0.0f);
    glUniform1i(glGetUniformLocation(pid, "uTonemap"),          1);  // preview tonemaps itself
    // IBL off in the preview; distinct units so cube/2D samplers don't clash on 0.
    glUniform1i(glGetUniformLocation(pid, "uHasIBL"),           0);
    glUniform1i(glGetUniformLocation(pid, "uIrradiance"),       8);
    glUniform1i(glGetUniformLocation(pid, "uPrefilter"),        9);
    glUniform1i(glGetUniformLocation(pid, "uBrdfLUT"),          10);
    glUniform1i(glGetUniformLocation(pid, "uSSAOEnabled"),      0);
    glUniform1i(glGetUniformLocation(pid, "uSSAO"),            11);

    int unit = 0;
    for (auto& [uname, texPath] : texBindings) {
        Texture* tex = texPath.empty() ? nullptr : TextureManager::GetOrLoad(texPath);
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, tex ? tex->GetID() : 0);
        glUniform1i(glGetUniformLocation(pid, uname.c_str()), unit);
        ++unit;
    }

    m_sphere->Draw();
    shader->Unbind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_fb->Unbind();
    return m_fb->GetColorTexture();
}

void MaterialPreviewRenderer::UpdatePreviewWithShader(
        const std::string& path, Shader* shader,
        const std::vector<std::pair<std::string, std::string>>& texBindings) {
    uint32_t rendered = RenderWithShader(shader, texBindings);
    if (!rendered) return;
    InvalidatePreview(path);             // drop stale thumbnail
    m_cache[path] = CopyToTexture(rendered);
}

uint32_t MaterialPreviewRenderer::Render(Material* mat) {
    if (!mat) return 0;

    m_fb->Bind();
    glViewport(0, 0, 188, 188);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.12f, 0.12f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 model      = glm::mat4(1.0f);
    glm::vec3 camPos     = glm::vec3(0.0f, 0.0f, 2.5f);
    glm::mat4 view       = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 projection = glm::perspective(glm::radians(40.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 mvp        = projection * view * model;

    m_shader->Bind();
    GLuint pid = m_shader->GetID();
    glUniformMatrix4fv(glGetUniformLocation(pid, "uMVP"),   1, GL_FALSE, glm::value_ptr(mvp));
    glUniformMatrix4fv(glGetUniformLocation(pid, "uModel"), 1, GL_FALSE, glm::value_ptr(model));

    glm::vec3 color(mat->color[0], mat->color[1], mat->color[2]);
    glm::vec3 emissive(mat->emissiveColor[0], mat->emissiveColor[1], mat->emissiveColor[2]);
    emissive *= mat->emissiveIntensity;

    glUniform3fv(glGetUniformLocation(pid, "uColor"),    1, glm::value_ptr(color));
    glUniform1f (glGetUniformLocation(pid, "uMetallic"),  mat->metallic);
    glUniform1f (glGetUniformLocation(pid, "uRoughness"), mat->roughness);
    glUniform3fv(glGetUniformLocation(pid, "uEmissive"),  1, glm::value_ptr(emissive));
    glUniform3fv(glGetUniformLocation(pid, "uCamPos"),    1, glm::value_ptr(camPos));

    // Albedo texture
    uint32_t albedoId = 0;
    if (!mat->albedoTexture.empty()) {
        Texture* tex = TextureManager::GetOrLoad(mat->albedoTexture);
        if (tex) albedoId = tex->GetID();
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, albedoId);
    glUniform1i(glGetUniformLocation(pid, "uAlbedoTex"),  0);
    glUniform1i(glGetUniformLocation(pid, "uHasAlbedo"),  albedoId ? 1 : 0);

    m_sphere->Draw();
    m_shader->Unbind();
    glBindTexture(GL_TEXTURE_2D, 0);

    m_fb->Unbind();
    return m_fb->GetColorTexture();
}
