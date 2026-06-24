#pragma once
#include <cstdint>
#include <string>
#include <glm/glm.hpp>

class CubeMesh;
class Shader;

// Environment cubemap + skybox rendering. Stage 1: procedural gradient sky and
// equirectangular-HDR loading + background skybox. (IBL added in stage 2.)
class EnvironmentMap {
public:
    EnvironmentMap();
    ~EnvironmentMap();

    void LoadProcedural();                  // gradient sky (default, no file needed)
    bool LoadHDR(const std::string& path);  // equirectangular .hdr → cubemap

    void RenderSkybox(const glm::mat4& view, const glm::mat4& projection);

    bool     IsValid()    const { return m_envCubemap != 0; }
    uint32_t GetCubemap() const { return m_envCubemap; }

    // IBL maps (valid after the environment is loaded).
    uint32_t GetIrradiance() const { return m_irradianceMap; }
    uint32_t GetPrefilter()  const { return m_prefilterMap; }
    uint32_t GetBrdfLUT()    const { return m_brdfLUT; }
    bool     HasIBL()        const { return m_irradianceMap != 0; }

private:
    void EnsureResources();
    void RenderCubeFaces(Shader* sh, uint32_t targetCube, int size, int mip); // draws 6 faces
    void GenerateIBL();                                                       // irradiance + prefilter

    uint32_t  m_envCubemap = 0;
    int       m_size       = 512;
    uint32_t  m_captureFBO = 0;

    // IBL precomputed maps
    uint32_t  m_irradianceMap = 0;   // 32   — diffuse
    uint32_t  m_prefilterMap  = 0;   // 128 + mips — specular
    uint32_t  m_brdfLUT       = 0;   // 512  — split-sum LUT (2D)
    uint32_t  m_quadVAO       = 0;
    uint32_t  m_quadVBO       = 0;

    CubeMesh* m_cube              = nullptr;
    Shader*   m_skyboxShader      = nullptr;
    Shader*   m_equirectShader    = nullptr;
    Shader*   m_proceduralShader  = nullptr;
    Shader*   m_irradianceShader  = nullptr;
    Shader*   m_prefilterShader   = nullptr;
    Shader*   m_brdfShader        = nullptr;
};
