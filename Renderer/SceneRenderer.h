#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <utility>
#include <glm/glm.hpp>
#include "../Scene/Scene.h"

class Framebuffer;
class Shader;
class PrimitiveMesh;
class ModelMesh;
class Grid;
class EnvironmentMap;

struct NodeShaderEntry {
    Shader*     shader = nullptr;
    // Each pair: (glsl uniform name, texture file path)
    std::vector<std::pair<std::string, std::string>> texBindings;
};

// One live particle (CPU simulated, world space).
struct Particle {
    glm::vec3 pos{ 0.0f };
    glm::vec3 vel{ 0.0f };
    float     life    = 0.0f;   // remaining seconds
    float     maxLife = 1.0f;
    float     size0 = 0.2f, size1 = 0.0f;
    glm::vec4 col0{ 1.0f }, col1{ 1.0f };
};

struct ParticlePool {
    std::vector<Particle> particles;
    float emitAccum = 0.0f;     // fractional particles pending emission
};

class SceneRenderer {
public:
    enum class AAMode { None, FXAA };   // expandable: SMAA, SSAA, TAA, ...

    SceneRenderer();
    ~SceneRenderer();

    uint32_t Render(Scene& scene, entt::entity selected,
        int width, int height,
        const glm::mat4& view, const glm::mat4& projection);

    void RegisterNodeShader(const std::string& matPath,
                            Shader* shader,
                            std::vector<std::pair<std::string,std::string>> texBindings);
    void UnregisterNodeShader(const std::string& matPath);

    // Environment / skybox
    void SetEnvironmentHDR(const std::string& hdrPath);

    // Save the last rendered viewport image to a 24-bit BMP. Returns success.
    bool SaveScreenshot(const std::string& path);

    // Animation clip names of a model (empty if static / not found). For the
    // Animator inspector dropdown.
    std::vector<std::string> ModelAnimNames(const std::string& emdlPath);

    // Post-processing
    void  SetExposure(float e) { m_exposure = e; }
    float GetExposure() const  { return m_exposure; }
    void  SetBloomEnabled(bool b) { m_bloomEnabled = b; }
    bool  GetBloomEnabled() const { return m_bloomEnabled; }
    void  SetBloomThreshold(float t) { m_bloomThreshold = t; }
    float GetBloomThreshold() const  { return m_bloomThreshold; }
    void  SetBloomIntensity(float i) { m_bloomIntensity = i; }
    float GetBloomIntensity() const  { return m_bloomIntensity; }
    void   SetAAMode(AAMode m) { m_aaMode = m; }
    AAMode GetAAMode() const   { return m_aaMode; }
    void  SetContrast(float v)    { m_contrast = v; }
    float GetContrast() const     { return m_contrast; }
    void  SetSaturation(float v)  { m_saturation = v; }
    float GetSaturation() const   { return m_saturation; }
    void  SetTemperature(float v) { m_temperature = v; }
    float GetTemperature() const  { return m_temperature; }
    void  SetVignette(float v)    { m_vignette = v; }
    float GetVignette() const     { return m_vignette; }
    void  SetSSAOEnabled(bool b)  { m_ssaoEnabled = b; }
    bool  GetSSAOEnabled() const  { return m_ssaoEnabled; }
    void  SetSSAORadius(float v)  { m_ssaoRadius = v; }
    float GetSSAORadius() const   { return m_ssaoRadius; }
    void  SetSSAOPower(float v)   { m_ssaoPower = v; }
    float GetSSAOPower() const    { return m_ssaoPower; }
    void  SetSSREnabled(bool b)   { m_ssrEnabled = b; }
    bool  GetSSREnabled() const   { return m_ssrEnabled; }
    void  SetSSRIntensity(float v){ m_ssrIntensity = v; }
    float GetSSRIntensity() const { return m_ssrIntensity; }

private:
    PrimitiveMesh* GetMeshForKind(MeshKind kind) const;
    ModelMesh*     GetOrLoadModel(const std::string& emdlPath);
    void UpdateAndRenderParticles(Scene& scene, const glm::mat4& view, const glm::mat4& projection);
    void           RenderShadowPass(Scene& scene);
    void           RenderPointShadowPass(Scene& scene,
                                         const glm::vec3& lightPos, float farPlane);
    void           EnsureBloomTargets(int width, int height);
    void           RenderBloom();   // bright-pass + blur into m_bloomTex[0]
    void           EnsureGBuffer(int width, int height);
    void           RenderGBuffer(Scene& scene,
                                 const glm::mat4& view, const glm::mat4& projection);
    void           RenderSSAO(const glm::mat4& projection);
    void           RenderSSR(const glm::mat4& projection);

    Framebuffer* m_framebuffer = nullptr;   // final LDR output (shown by ImGui)
    Framebuffer* m_hdrFB       = nullptr;   // HDR scene buffer (RGBA16F)
    Framebuffer* m_ldrFB       = nullptr;   // tonemapped LDR, input to FXAA
    Shader* m_shader = nullptr;
    Shader* m_skinnedShader = nullptr;      // GPU skinning variant of m_shader
    Shader* m_particleShader = nullptr;     // billboarded particles
    std::unordered_map<entt::entity, ParticlePool> m_particlePools;
    uint32_t m_particleVao = 0;
    uint32_t m_particleVbo = 0;
    double   m_lastParticleTime = 0.0;
    Shader* m_lineShader = nullptr;
    Shader* m_outlineShader = nullptr;
    Shader* m_depthShader = nullptr;
    Shader* m_pointDepthShader = nullptr;
    Shader* m_postShader = nullptr;         // tonemap + exposure + gamma + bloom composite
    Shader* m_brightShader = nullptr;       // bloom bright-pass
    Shader* m_blurShader = nullptr;         // bloom gaussian blur
    Shader* m_fxaaShader = nullptr;         // FXAA anti-aliasing

    // Fullscreen quad for post-processing
    uint32_t m_quadVAO = 0;
    uint32_t m_quadVBO = 0;

    // Bloom ping-pong targets (half-resolution, HDR)
    uint32_t m_bloomFBO[2] = { 0, 0 };
    uint32_t m_bloomTex[2] = { 0, 0 };
    int      m_bloomW = 0;
    int      m_bloomH = 0;

    // Post-process params
    float    m_exposure       = 1.0f;
    bool     m_bloomEnabled   = true;
    float    m_bloomThreshold = 1.0f;
    float    m_bloomIntensity = 0.6f;
    AAMode   m_aaMode         = AAMode::FXAA;

    // Color grading
    float    m_contrast    = 1.0f;
    float    m_saturation  = 1.0f;
    float    m_temperature = 0.0f;
    float    m_vignette    = 0.0f;

    // SSAO + G-buffer (view-space normal/position prepass)
    Shader*  m_geomShader     = nullptr;
    Shader*  m_ssaoShader     = nullptr;
    Shader*  m_ssaoBlurShader = nullptr;
    uint32_t m_gFBO        = 0, m_gNormalTex = 0, m_gPosTex = 0, m_gDepthRBO = 0;
    uint32_t m_ssaoFBO     = 0, m_ssaoTex = 0;
    uint32_t m_ssaoBlurFBO = 0, m_ssaoBlurTex = 0;
    uint32_t m_ssaoNoiseTex = 0;
    int      m_gW = 0, m_gH = 0;
    std::vector<glm::vec3> m_ssaoKernel;
    bool     m_ssaoEnabled = true;
    float    m_ssaoRadius  = 0.5f;
    float    m_ssaoBias    = 0.025f;
    float    m_ssaoPower   = 1.0f;

    // SSR (screen-space reflections) — reuses the G-buffer + HDR color
    Shader*  m_ssrShader = nullptr;
    uint32_t m_ssrFBO = 0, m_ssrTex = 0;
    bool     m_ssrEnabled   = true;
    float    m_ssrIntensity = 0.5f;
    float    m_ssrMaxDist   = 12.0f;
    int      m_ssrSteps     = 48;
    float    m_ssrThickness = 0.6f;

    // Shadow map for the single directional/spot caster
    uint32_t  m_shadowFBO  = 0;
    uint32_t  m_shadowTex  = 0;
    int       m_shadowSize = 2048;
    glm::mat4 m_lightSpace = glm::mat4(1.0f);

    // Cube shadow map for a single point-light caster
    uint32_t  m_pointFBO      = 0;
    uint32_t  m_pointCubeTex  = 0;
    uint32_t  m_pointDepthRBO = 0;
    int       m_pointSize     = 1024;

    PrimitiveMesh* m_cube = nullptr;
    PrimitiveMesh* m_sphere = nullptr;
    PrimitiveMesh* m_plane = nullptr;
    Grid* m_grid = nullptr;
    EnvironmentMap* m_env = nullptr;

    std::unordered_map<std::string, ModelMesh*>     m_modelCache;
    std::unordered_map<std::string, NodeShaderEntry> m_nodeShaders;
};