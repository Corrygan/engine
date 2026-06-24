#include "EnvironmentMap.h"
#include "Mesh.h"
#include "Shader.h"
#include "glad/gl.h"
#include <glm/gtc/matrix_transform.hpp>
#include <stb_image.h>
#include <iostream>

namespace {
    // Vertex shader shared by the cube-capture passes: position is the direction.
    const char* kCaptureVert = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uView;
uniform mat4 uProj;
out vec3 vDir;
void main() {
    vDir = aPos;
    gl_Position = uProj * uView * vec4(aPos, 1.0);
}
)";

    // Equirectangular HDR → cubemap.
    const char* kEquirectFrag = R"(
#version 330 core
in vec3 vDir;
out vec4 FragColor;
uniform sampler2D uEquirect;
const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 sampleSpherical(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(clamp(v.y, -1.0, 1.0)));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}
void main() {
    FragColor = vec4(texture(uEquirect, sampleSpherical(normalize(vDir))).rgb, 1.0);
}
)";

    // Procedural gradient sky (linear HDR-ish values, tonemapped by the skybox).
    const char* kProceduralFrag = R"(
#version 330 core
in vec3 vDir;
out vec4 FragColor;
void main() {
    vec3  d = normalize(vDir);
    vec3 horizon = vec3(0.62, 0.70, 0.82);
    vec3 zenith  = vec3(0.16, 0.30, 0.58);
    vec3 ground  = vec3(0.10, 0.10, 0.12);
    vec3 col;
    if (d.y >= 0.0) col = mix(horizon, zenith, pow(d.y, 0.55));
    else            col = mix(horizon, ground, pow(-d.y, 0.4));
    // Soft sun toward +x +y.
    float sun = pow(max(dot(d, normalize(vec3(0.6, 0.7, 0.4))), 0.0), 220.0);
    col += vec3(1.4, 1.25, 1.0) * sun;
    FragColor = vec4(col, 1.0);
}
)";

    const char* kSkyboxVert = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uView;
uniform mat4 uProj;
out vec3 vDir;
void main() {
    vDir = aPos;
    mat4 rotView = mat4(mat3(uView));      // drop translation
    vec4 clip = uProj * rotView * vec4(aPos, 1.0);
    gl_Position = clip.xyww;               // force depth = 1.0 (far plane)
}
)";

    const char* kSkyboxFrag = R"(
#version 330 core
in vec3 vDir;
out vec4 FragColor;
uniform samplerCube uEnv;
void main() {
    // Output linear HDR; the scene post pass applies tonemap + gamma.
    FragColor = vec4(texture(uEnv, normalize(vDir)).rgb, 1.0);
}
)";

    // ── IBL: diffuse irradiance convolution ──────────────────────────────
    const char* kIrradianceFrag = R"(
#version 330 core
in vec3 vDir;
out vec4 FragColor;
uniform samplerCube uEnv;
const float PI = 3.14159265359;
void main() {
    vec3 N = normalize(vDir);
    vec3 up = abs(N.z) < 0.999 ? vec3(0,0,1) : vec3(1,0,0);
    vec3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));
    vec3 irradiance = vec3(0.0);
    float samples = 0.0;
    const float d = 0.025;
    for (float phi = 0.0; phi < 2.0*PI; phi += d) {
        for (float theta = 0.0; theta < 0.5*PI; theta += d) {
            vec3 t = vec3(sin(theta)*cos(phi), sin(theta)*sin(phi), cos(theta));
            vec3 s = t.x*right + t.y*up + t.z*N;
            irradiance += texture(uEnv, s).rgb * cos(theta) * sin(theta);
            samples++;
        }
    }
    FragColor = vec4(PI * irradiance / samples, 1.0);
}
)";

    // ── IBL: specular prefilter (GGX importance sampling) ────────────────
    const char* kPrefilterFrag = R"(
#version 330 core
in vec3 vDir;
out vec4 FragColor;
uniform samplerCube uEnv;
uniform float uRoughness;
const float PI = 3.14159265359;
float RadicalInverse_VdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}
vec2 Hammersley(uint i, uint N) { return vec2(float(i)/float(N), RadicalInverse_VdC(i)); }
vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness) {
    float a = roughness*roughness;
    float phi = 2.0*PI*Xi.x;
    float cosT = sqrt((1.0-Xi.y)/(1.0+(a*a-1.0)*Xi.y));
    float sinT = sqrt(1.0-cosT*cosT);
    vec3 H = vec3(cos(phi)*sinT, sin(phi)*sinT, cosT);
    vec3 up = abs(N.z)<0.999 ? vec3(0,0,1) : vec3(1,0,0);
    vec3 tangent = normalize(cross(up,N));
    vec3 bitangent = cross(N,tangent);
    return normalize(tangent*H.x + bitangent*H.y + N*H.z);
}
void main() {
    vec3 N = normalize(vDir);
    vec3 V = N;
    const uint COUNT = 1024u;
    vec3 color = vec3(0.0);
    float weight = 0.0;
    for (uint i = 0u; i < COUNT; ++i) {
        vec2 Xi = Hammersley(i, COUNT);
        vec3 H = ImportanceSampleGGX(Xi, N, uRoughness);
        vec3 L = normalize(2.0*dot(V,H)*H - V);
        float NdotL = max(dot(N,L), 0.0);
        if (NdotL > 0.0) { color += texture(uEnv, L).rgb * NdotL; weight += NdotL; }
    }
    FragColor = vec4(color / max(weight, 0.001), 1.0);
}
)";

    // ── IBL: BRDF integration LUT (fullscreen quad) ──────────────────────
    const char* kBrdfVert = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
out vec2 vUV;
void main() { vUV = aUV; gl_Position = vec4(aPos, 0.0, 1.0); }
)";

    const char* kBrdfFrag = R"(
#version 330 core
in vec2 vUV;
out vec2 FragColor;
const float PI = 3.14159265359;
float RadicalInverse_VdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}
vec2 Hammersley(uint i, uint N) { return vec2(float(i)/float(N), RadicalInverse_VdC(i)); }
vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness) {
    float a = roughness*roughness;
    float phi = 2.0*PI*Xi.x;
    float cosT = sqrt((1.0-Xi.y)/(1.0+(a*a-1.0)*Xi.y));
    float sinT = sqrt(1.0-cosT*cosT);
    vec3 H = vec3(cos(phi)*sinT, sin(phi)*sinT, cosT);
    vec3 up = abs(N.z)<0.999 ? vec3(0,0,1) : vec3(1,0,0);
    vec3 tangent = normalize(cross(up,N));
    vec3 bitangent = cross(N,tangent);
    return normalize(tangent*H.x + bitangent*H.y + N*H.z);
}
float G_SchlickGGX(float NdotV, float roughness) {
    float k = (roughness*roughness)/2.0;
    return NdotV/(NdotV*(1.0-k)+k);
}
float G_Smith(vec3 N, vec3 V, vec3 L, float roughness) {
    return G_SchlickGGX(max(dot(N,V),0.0), roughness) * G_SchlickGGX(max(dot(N,L),0.0), roughness);
}
vec2 IntegrateBRDF(float NdotV, float roughness) {
    vec3 V = vec3(sqrt(1.0-NdotV*NdotV), 0.0, NdotV);
    float A = 0.0, B = 0.0;
    vec3 N = vec3(0,0,1);
    const uint COUNT = 1024u;
    for (uint i = 0u; i < COUNT; ++i) {
        vec2 Xi = Hammersley(i, COUNT);
        vec3 H = ImportanceSampleGGX(Xi, N, roughness);
        vec3 L = normalize(2.0*dot(V,H)*H - V);
        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V,H), 0.0);
        if (NdotL > 0.0) {
            float G = G_Smith(N, V, L, roughness);
            float G_Vis = (G*VdotH)/(NdotH*NdotV);
            float Fc = pow(1.0-VdotH, 5.0);
            A += (1.0-Fc)*G_Vis;
            B += Fc*G_Vis;
        }
    }
    return vec2(A, B) / float(COUNT);
}
void main() { FragColor = IntegrateBRDF(vUV.x, vUV.y); }
)";
}

EnvironmentMap::EnvironmentMap() {}

EnvironmentMap::~EnvironmentMap() {
    if (m_envCubemap)    glDeleteTextures(1, &m_envCubemap);
    if (m_irradianceMap) glDeleteTextures(1, &m_irradianceMap);
    if (m_prefilterMap)  glDeleteTextures(1, &m_prefilterMap);
    if (m_brdfLUT)       glDeleteTextures(1, &m_brdfLUT);
    if (m_captureFBO)    glDeleteFramebuffers(1, &m_captureFBO);
    if (m_quadVBO)       glDeleteBuffers(1, &m_quadVBO);
    if (m_quadVAO)       glDeleteVertexArrays(1, &m_quadVAO);
    delete m_cube;
    delete m_skyboxShader;
    delete m_equirectShader;
    delete m_proceduralShader;
    delete m_irradianceShader;
    delete m_prefilterShader;
    delete m_brdfShader;
}

void EnvironmentMap::EnsureResources() {
    if (m_envCubemap) return;

    m_cube             = new CubeMesh();
    m_skyboxShader     = new Shader(kSkyboxVert,  kSkyboxFrag);
    m_equirectShader   = new Shader(kCaptureVert, kEquirectFrag);
    m_proceduralShader = new Shader(kCaptureVert, kProceduralFrag);
    m_irradianceShader = new Shader(kCaptureVert, kIrradianceFrag);
    m_prefilterShader  = new Shader(kCaptureVert, kPrefilterFrag);
    m_brdfShader       = new Shader(kBrdfVert,    kBrdfFrag);

    // Fullscreen quad for the BRDF LUT pass.
    const float quad[] = {
        -1.0f,  1.0f, 0.0f, 1.0f,   -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,    1.0f, -1.0f, 1.0f, 0.0f,
    };
    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
    glBindVertexArray(0);

    glGenTextures(1, &m_envCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);
    for (int f = 0; f < 6; ++f)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, GL_RGB16F,
                     m_size, m_size, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenFramebuffers(1, &m_captureFBO);
}

void EnvironmentMap::RenderCubeFaces(Shader* sh, uint32_t targetCube, int size, int mip) {
    static const glm::vec3 origin(0.0f);
    glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 views[6] = {
        glm::lookAt(origin, glm::vec3( 1, 0, 0), glm::vec3(0,-1, 0)),
        glm::lookAt(origin, glm::vec3(-1, 0, 0), glm::vec3(0,-1, 0)),
        glm::lookAt(origin, glm::vec3( 0, 1, 0), glm::vec3(0, 0, 1)),
        glm::lookAt(origin, glm::vec3( 0,-1, 0), glm::vec3(0, 0,-1)),
        glm::lookAt(origin, glm::vec3( 0, 0, 1), glm::vec3(0,-1, 0)),
        glm::lookAt(origin, glm::vec3( 0, 0,-1), glm::vec3(0,-1, 0)),
    };

    glViewport(0, 0, size, size);
    sh->SetMat4("uProj", proj);
    for (int f = 0; f < 6; ++f) {
        sh->SetMat4("uView", views[f]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, targetCube, mip);
        glClear(GL_COLOR_BUFFER_BIT);
        m_cube->Draw();
    }
}

void EnvironmentMap::LoadProcedural() {
    EnsureResources();

    GLint prevVp[4]; glGetIntegerv(GL_VIEWPORT, prevVp);
    glBindFramebuffer(GL_FRAMEBUFFER, m_captureFBO);
    glDisable(GL_DEPTH_TEST);
    m_proceduralShader->Bind();
    RenderCubeFaces(m_proceduralShader, m_envCubemap, m_size, 0);
    m_proceduralShader->Unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_DEPTH_TEST);
    glViewport(prevVp[0], prevVp[1], prevVp[2], prevVp[3]);

    glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    GenerateIBL();
}

bool EnvironmentMap::LoadHDR(const std::string& path) {
    EnsureResources();

    stbi_set_flip_vertically_on_load(true);
    int w, h, ch;
    float* data = stbi_loadf(path.c_str(), &w, &h, &ch, 3);
    stbi_set_flip_vertically_on_load(false);
    if (!data) {
        std::cerr << "[Env] Failed to load HDR: " << path << std::endl;
        return false;
    }

    uint32_t equirect;
    glGenTextures(1, &equirect);
    glBindTexture(GL_TEXTURE_2D, equirect);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(data);

    GLint prevVp[4]; glGetIntegerv(GL_VIEWPORT, prevVp);
    glBindFramebuffer(GL_FRAMEBUFFER, m_captureFBO);
    glDisable(GL_DEPTH_TEST);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, equirect);
    m_equirectShader->Bind();
    m_equirectShader->SetInt("uEquirect", 0);
    RenderCubeFaces(m_equirectShader, m_envCubemap, m_size, 0);
    m_equirectShader->Unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_DEPTH_TEST);
    glViewport(prevVp[0], prevVp[1], prevVp[2], prevVp[3]);

    glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    glDeleteTextures(1, &equirect);

    GenerateIBL();
    return true;
}

void EnvironmentMap::GenerateIBL() {
    // Allocate the IBL maps once.
    if (!m_irradianceMap) {
        glGenTextures(1, &m_irradianceMap);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_irradianceMap);
        for (int f = 0; f < 6; ++f)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, GL_RGB16F, 32, 32, 0,
                         GL_RGB, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    if (!m_prefilterMap) {
        glGenTextures(1, &m_prefilterMap);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_prefilterMap);
        for (int f = 0; f < 6; ++f)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, GL_RGB16F, 128, 128, 0,
                         GL_RGB, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);   // allocate mip chain
    }
    if (!m_brdfLUT) {
        glGenTextures(1, &m_brdfLUT);
        glBindTexture(GL_TEXTURE_2D, m_brdfLUT);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    GLint prevVp[4]; glGetIntegerv(GL_VIEWPORT, prevVp);
    glBindFramebuffer(GL_FRAMEBUFFER, m_captureFBO);
    glDisable(GL_DEPTH_TEST);

    // 1) Diffuse irradiance.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);
    m_irradianceShader->Bind();
    m_irradianceShader->SetInt("uEnv", 0);
    RenderCubeFaces(m_irradianceShader, m_irradianceMap, 32, 0);
    m_irradianceShader->Unbind();

    // 2) Specular prefilter, one roughness level per mip.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);
    m_prefilterShader->Bind();
    m_prefilterShader->SetInt("uEnv", 0);
    const int maxMip = 5;
    for (int mip = 0; mip < maxMip; ++mip) {
        int   sz   = 128 >> mip;
        float roughness = (float)mip / (float)(maxMip - 1);
        m_prefilterShader->SetFloat("uRoughness", roughness);
        RenderCubeFaces(m_prefilterShader, m_prefilterMap, sz, mip);
    }
    m_prefilterShader->Unbind();

    // 3) BRDF integration LUT (fullscreen quad).
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_brdfLUT, 0);
    glViewport(0, 0, 512, 512);
    glClear(GL_COLOR_BUFFER_BIT);
    m_brdfShader->Bind();
    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    m_brdfShader->Unbind();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_DEPTH_TEST);
    glViewport(prevVp[0], prevVp[1], prevVp[2], prevVp[3]);
}

void EnvironmentMap::RenderSkybox(const glm::mat4& view, const glm::mat4& projection) {
    if (!m_envCubemap) return;

    glDepthFunc(GL_LEQUAL);
    m_skyboxShader->Bind();
    m_skyboxShader->SetMat4("uView", view);
    m_skyboxShader->SetMat4("uProj", projection);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);
    m_skyboxShader->SetInt("uEnv", 0);
    m_cube->Draw();
    m_skyboxShader->Unbind();
    glDepthFunc(GL_LESS);
}
