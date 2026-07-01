#include "SceneRenderer.h"
#include "Framebuffer.h"
#include "Shader.h"
#include "Mesh.h"
#include "ModelMesh.h"
#include "MaterialManager.h"
#include "TextureManager.h"
#include "Grid.h"
#include "EnvironmentMap.h"
#include "LightingGLSL.h"
#include "glad/gl.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>
#include <cstdio>
#include <cmath>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <random>

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

    // Skinned variant: same outputs/uniforms as the lit vertex shader plus GPU
    // skinning from a per-frame bone-matrix palette. Shares the fragment shader.
    const char* kSkinnedVertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in vec3 aBitangent;
layout(location = 5) in ivec4 aBoneIds;
layout(location = 6) in vec4  aWeights;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

const int MAX_BONES = 256;
uniform mat4 uBones[MAX_BONES];

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;
out vec3 vTangent;
out vec3 vBitangent;

void main() {
    float total = aWeights.x + aWeights.y + aWeights.z + aWeights.w;
    mat4 skin;
    if (total > 0.0001) {
        ivec4 ids = clamp(aBoneIds, 0, MAX_BONES - 1);
        skin  = aWeights.x * uBones[ids.x];
        skin += aWeights.y * uBones[ids.y];
        skin += aWeights.z * uBones[ids.z];
        skin += aWeights.w * uBones[ids.w];
    } else {
        skin = mat4(1.0);
    }
    mat4 world = uModel * skin;
    vec4 worldPos = world * vec4(aPos, 1.0);
    vWorldPos  = worldPos.xyz;
    mat3 nm    = mat3(transpose(inverse(world)));
    vNormal    = nm * aNormal;
    vTangent   = nm * aTangent;
    vBitangent = nm * aBitangent;
    vUV        = aUV;
    gl_Position = uProjection * uView * worldPos;
}
)";

    const char* kParticleVertSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
uniform mat4 uView;
uniform mat4 uProjection;
out vec2 vUV;
out vec4 vColor;
void main() {
    vUV = aUV;
    vColor = aColor;
    gl_Position = uProjection * uView * vec4(aPos, 1.0);
}
)";

    const char* kParticleFragSrc = R"(
#version 330 core
in vec2 vUV;
in vec4 vColor;
out vec4 FragColor;
uniform sampler2D uTex;
uniform int uHasTex;
void main() {
    vec4 c = vColor;
    if (uHasTex == 1) {
        c *= texture(uTex, vUV);
    } else {
        float d = length(vUV - vec2(0.5)) * 2.0;   // soft round dot
        c.a *= clamp(1.0 - d, 0.0, 1.0);
    }
    if (c.a <= 0.002) discard;
    FragColor = c;
}
)";

    // Assembled at construction: kFragHeader + kLightingGLSL + kFragMain.
    const char* kFragHeader = R"(
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
)";

    const char* kFragMain = R"(
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

    vec3 Lo      = shadePBR(N, V, albedo, metallic, roughness);
    vec3 ambient = iblAmbient(N, V, albedo, metallic, roughness, ao);
    vec3 color   = ambient + Lo + uEmissive;

    FragColor = vec4(color, 1.0);     // linear HDR — tonemapped in the post pass
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

    // Depth-only pass for shadow maps.
    const char* kDepthVertSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uLightSpace;
uniform mat4 uModel;
void main() {
    gl_Position = uLightSpace * uModel * vec4(aPos, 1.0);
}
)";

    const char* kDepthFragSrc = R"(
#version 330 core
void main() {}
)";

    // Point-light depth: store normalized world distance from the light per face.
    const char* kPointDepthVertSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uViewProj;
uniform mat4 uModel;
out vec3 vWorld;
void main() {
    vec4 w = uModel * vec4(aPos, 1.0);
    vWorld = w.xyz;
    gl_Position = uViewProj * w;
}
)";

    const char* kPointDepthFragSrc = R"(
#version 330 core
in vec3 vWorld;
uniform vec3  uLightPos;
uniform float uFarPlane;
out vec4 FragColor;
void main() {
    FragColor = vec4(length(vWorld - uLightPos) / uFarPlane, 0.0, 0.0, 1.0);
}
)";

    // ── Post-processing: tonemap (ACES) + exposure + gamma ───────────────
    const char* kPostVertSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
out vec2 vUV;
void main() { vUV = aUV; gl_Position = vec4(aPos, 0.0, 1.0); }
)";

    const char* kPostFragSrc = R"(
#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform float uExposure;
uniform float uBloomIntensity;
uniform int   uHasBloom;
uniform float uContrast;       // 1 = neutral
uniform float uSaturation;     // 1 = neutral
uniform float uTemperature;    // -1 cool .. +1 warm
uniform float uVignette;       // 0 = off .. 1 = strong
uniform sampler2D uSSR;
uniform int   uSSREnabled;
uniform float uSSRIntensity;
vec3 ACESFilm(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}
void main() {
    vec3 hdr = texture(uScene, vUV).rgb;
    if (uSSREnabled == 1) {
        vec4 r = texture(uSSR, vUV);
        hdr += r.rgb * r.a * uSSRIntensity;     // screen-space reflections
    }
    if (uHasBloom == 1) hdr += texture(uBloom, vUV).rgb * uBloomIntensity;
    hdr *= uExposure;
    vec3 col = ACESFilm(hdr);
    col = pow(col, vec3(1.0 / 2.2));

    // ── Color grading ──
    col.r += uTemperature * 0.10;          // white balance (warm/cool)
    col.b -= uTemperature * 0.10;
    col = clamp(col, 0.0, 1.0);
    col = (col - 0.5) * uContrast + 0.5;    // contrast
    float l = dot(col, vec3(0.2126, 0.7152, 0.0722));
    col = mix(vec3(l), col, uSaturation);   // saturation
    col = clamp(col, 0.0, 1.0);

    // ── Vignette ──
    float dist = length(vUV - 0.5) * 1.41421356;   // 0 center .. ~1 corner
    float vig  = smoothstep(1.0, 0.35, dist);
    col *= mix(1.0, vig, uVignette);

    FragColor = vec4(col, 1.0);
}
)";

    // Bloom bright-pass: keep only the energy above the threshold.
    const char* kBrightFragSrc = R"(
#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uScene;
uniform float uThreshold;
void main() {
    vec3 c = texture(uScene, vUV).rgb;
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    float contrib = max(l - uThreshold, 0.0) / max(l, 1e-4);
    FragColor = vec4(c * contrib, 1.0);
}
)";

    // ── G-buffer prepass: view-space normal + position (for SSAO / SSR) ──
    const char* kGeomVertSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
out vec3 vViewPos;
out vec3 vViewNormal;
void main() {
    mat4 mv = uView * uModel;
    vec4 vp = mv * vec4(aPos, 1.0);
    vViewPos    = vp.xyz;
    vViewNormal = mat3(transpose(inverse(mv))) * aNormal;
    gl_Position = uProj * vp;
}
)";

    const char* kGeomFragSrc = R"(
#version 330 core
layout(location = 0) out vec4 gNormal;
layout(location = 1) out vec4 gPosition;
in vec3 vViewPos;
in vec3 vViewNormal;
void main() {
    gNormal   = vec4(normalize(vViewNormal), 1.0);
    gPosition = vec4(vViewPos, 1.0);
}
)";

    // SSAO: hemisphere kernel sampling against the view-space G-buffer.
    const char* kSsaoFragSrc = R"(
#version 330 core
in vec2 vUV;
out float FragColor;
uniform sampler2D gNormal;
uniform sampler2D gPosition;
uniform sampler2D uNoise;
uniform vec3  uKernel[64];
uniform mat4  uProj;
uniform vec2  uNoiseScale;
uniform float uRadius;
uniform float uBias;
uniform float uPower;
const int KERNEL = 64;
void main() {
    vec3 fragPos = texture(gPosition, vUV).xyz;
    vec3 normal  = normalize(texture(gNormal, vUV).xyz);
    vec3 randv   = normalize(texture(uNoise, vUV * uNoiseScale).xyz);
    vec3 tangent = normalize(randv - normal * dot(randv, normal));
    vec3 bitan   = cross(normal, tangent);
    mat3 TBN     = mat3(tangent, bitan, normal);

    float occ = 0.0;
    for (int i = 0; i < KERNEL; ++i) {
        vec3 samplePos = fragPos + (TBN * uKernel[i]) * uRadius;
        vec4 off = uProj * vec4(samplePos, 1.0);
        off.xyz /= off.w;
        off.xyz  = off.xyz * 0.5 + 0.5;
        float sampleDepth = texture(gPosition, off.xy).z;
        float rangeCheck  = smoothstep(0.0, 1.0, uRadius / abs(fragPos.z - sampleDepth));
        occ += (sampleDepth >= samplePos.z + uBias ? 1.0 : 0.0) * rangeCheck;
    }
    occ = 1.0 - occ / float(KERNEL);
    FragColor = pow(clamp(occ, 0.0, 1.0), uPower);
}
)";

    const char* kSsaoBlurFragSrc = R"(
#version 330 core
in vec2 vUV;
out float FragColor;
uniform sampler2D uSSAOInput;
void main() {
    vec2 texel = 1.0 / vec2(textureSize(uSSAOInput, 0));
    float result = 0.0;
    for (int x = -2; x < 2; ++x)
        for (int y = -2; y < 2; ++y)
            result += texture(uSSAOInput, vUV + vec2(float(x), float(y)) * texel).r;
    FragColor = result / 16.0;
}
)";

    // ── SSR: view-space ray march against the G-buffer, sample HDR color ──
    const char* kSsrFragSrc = R"(
#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D gPosition;   // view-space position
uniform sampler2D gNormal;     // view-space normal
uniform sampler2D uScene;      // HDR lit color
uniform mat4  uProj;
uniform float uMaxDistance;
uniform int   uSteps;
uniform float uThickness;
void main() {
    vec3 P = texture(gPosition, vUV).xyz;
    if (P.z > -1e-4) { FragColor = vec4(0.0); return; }      // no geometry (background)
    vec3 N = normalize(texture(gNormal, vUV).xyz);
    vec3 V = normalize(P);                                    // camera(0) → point
    vec3 R = normalize(reflect(V, N));

    float stepLen = uMaxDistance / float(uSteps);
    vec3  rayPos  = P;
    vec3  hitCol  = vec3(0.0);
    float hit     = 0.0;
    for (int i = 0; i < uSteps; ++i) {
        rayPos += R * stepLen;
        if (rayPos.z > -1e-4) break;                          // behind camera
        vec4 clip = uProj * vec4(rayPos, 1.0);
        vec2 uv = (clip.xy / clip.w) * 0.5 + 0.5;
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) break;
        float sceneZ = texture(gPosition, uv).z;
        if (sceneZ > -1e-4) continue;                         // sample has no geometry
        float diff = rayPos.z - sceneZ;                       // <0 = ray behind surface
        if (diff < 0.0 && diff > -uThickness) {
            vec2 e = smoothstep(0.0, 0.15, uv) * smoothstep(0.0, 0.15, 1.0 - uv);
            hit    = e.x * e.y;                               // fade near screen edges
            hitCol = texture(uScene, uv).rgb;
            break;
        }
    }
    float fres = pow(1.0 - max(dot(N, -V), 0.0), 3.0);        // grazing → stronger
    FragColor = vec4(hitCol, hit * fres);
}
)";

    // FXAA (Lottes, compact) — edge-aware smoothing on the tonemapped image.
    const char* kFxaaFragSrc = R"(
#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uTex;
uniform vec2 uRcpFrame;          // 1.0 / resolution
float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }
void main() {
    const float SPAN_MAX   = 8.0;
    const float REDUCE_MUL = 1.0 / 8.0;
    const float REDUCE_MIN = 1.0 / 128.0;

    vec3 rgbM  = texture(uTex, vUV).rgb;
    vec3 rgbNW = texture(uTex, vUV + vec2(-1.0,-1.0) * uRcpFrame).rgb;
    vec3 rgbNE = texture(uTex, vUV + vec2( 1.0,-1.0) * uRcpFrame).rgb;
    vec3 rgbSW = texture(uTex, vUV + vec2(-1.0, 1.0) * uRcpFrame).rgb;
    vec3 rgbSE = texture(uTex, vUV + vec2( 1.0, 1.0) * uRcpFrame).rgb;

    float lM  = luma(rgbM),  lNW = luma(rgbNW), lNE = luma(rgbNE);
    float lSW = luma(rgbSW), lSE = luma(rgbSE);
    float lMin = min(lM, min(min(lNW, lNE), min(lSW, lSE)));
    float lMax = max(lM, max(max(lNW, lNE), max(lSW, lSE)));

    vec2 dir;
    dir.x = -((lNW + lNE) - (lSW + lSE));
    dir.y =  ((lNW + lSW) - (lNE + lSE));

    float dirReduce = max((lNW + lNE + lSW + lSE) * 0.25 * REDUCE_MUL, REDUCE_MIN);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, vec2(-SPAN_MAX), vec2(SPAN_MAX)) * uRcpFrame;

    vec3 rgbA = 0.5 * (texture(uTex, vUV + dir * (1.0/3.0 - 0.5)).rgb +
                       texture(uTex, vUV + dir * (2.0/3.0 - 0.5)).rgb);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (texture(uTex, vUV + dir * -0.5).rgb +
                                     texture(uTex, vUV + dir *  0.5).rgb);

    float lB = luma(rgbB);
    FragColor = (lB < lMin || lB > lMax) ? vec4(rgbA, 1.0) : vec4(rgbB, 1.0);
}
)";

    // Bloom separable Gaussian blur (9-tap).
    const char* kBlurFragSrc = R"(
#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uImage;
uniform int uHorizontal;
const float w[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
void main() {
    vec2 texel = 1.0 / vec2(textureSize(uImage, 0));
    vec3 result = texture(uImage, vUV).rgb * w[0];
    if (uHorizontal == 1) {
        for (int i = 1; i < 5; ++i) {
            result += texture(uImage, vUV + vec2(texel.x * float(i), 0.0)).rgb * w[i];
            result += texture(uImage, vUV - vec2(texel.x * float(i), 0.0)).rgb * w[i];
        }
    } else {
        for (int i = 1; i < 5; ++i) {
            result += texture(uImage, vUV + vec2(0.0, texel.y * float(i))).rgb * w[i];
            result += texture(uImage, vUV - vec2(0.0, texel.y * float(i))).rgb * w[i];
        }
    }
    FragColor = vec4(result, 1.0);
}
)";

    constexpr int kMaxLights = 8;

    struct GpuLight {
        int       type;          // 0=dir, 1=point, 2=spot
        glm::vec3 pos;
        glm::vec3 dir;
        glm::vec3 color;         // color * intensity
        float     range;
        float     innerCos;
        float     outerCos;
    };

    std::vector<GpuLight> CollectLights(Scene& scene) {
        std::vector<GpuLight> lights;
        const entt::registry& reg = scene.Reg();
        auto view = reg.view<TransformComponent, LightComponent>();
        for (auto e : view) {
            if ((int)lights.size() >= kMaxLights) break;
            const LightComponent& l = view.get<LightComponent>(e);

            // World transform so parented lights inherit the parent's pose.
            glm::mat4 world = WorldMatrixOf(reg, e);
            glm::vec3 fwd = glm::normalize(glm::mat3(world) * glm::vec3(0.0f, 0.0f, -1.0f));
            GpuLight gl;
            gl.type     = l.type;
            gl.pos      = glm::vec3(world[3]);
            gl.dir      = fwd;
            gl.color    = l.color * l.intensity;
            gl.range    = l.range;
            gl.innerCos = std::cos(glm::radians(l.innerDeg));
            gl.outerCos = std::cos(glm::radians(l.outerDeg));
            lights.push_back(gl);
        }
        return lights;
    }

    float NowSeconds() {
        static auto start = std::chrono::steady_clock::now();
        return std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();
    }

    // Minimal 24-bit BMP writer (no deps). rgba rows are bottom-up (GL order),
    // which matches BMP's bottom-up layout, so no flip is needed.
    bool WriteBMP(const std::string& path, int w, int h, const unsigned char* rgba) {
        std::ofstream f(path, std::ios::binary);
        if (!f) return false;
        const int rowSize  = (w * 3 + 3) & ~3;          // padded to 4 bytes
        const int dataSize = rowSize * h;
        const int fileSize = 54 + dataSize;

        unsigned char fh[14] = {0};
        fh[0] = 'B'; fh[1] = 'M';
        fh[2] = fileSize & 0xFF; fh[3] = (fileSize >> 8) & 0xFF;
        fh[4] = (fileSize >> 16) & 0xFF; fh[5] = (fileSize >> 24) & 0xFF;
        fh[10] = 54;
        f.write(reinterpret_cast<char*>(fh), 14);

        unsigned char ih[40] = {0};
        ih[0] = 40;
        ih[4] = w & 0xFF; ih[5] = (w >> 8) & 0xFF; ih[6] = (w >> 16) & 0xFF; ih[7] = (w >> 24) & 0xFF;
        ih[8] = h & 0xFF; ih[9] = (h >> 8) & 0xFF; ih[10] = (h >> 16) & 0xFF; ih[11] = (h >> 24) & 0xFF;
        ih[12] = 1;        // planes
        ih[14] = 24;       // bits per pixel
        f.write(reinterpret_cast<char*>(ih), 40);

        std::vector<unsigned char> row(rowSize, 0);
        for (int y = 0; y < h; ++y) {
            const unsigned char* src = rgba + (size_t)y * w * 4;
            for (int x = 0; x < w; ++x) {
                row[x * 3 + 0] = src[x * 4 + 2];  // B
                row[x * 3 + 1] = src[x * 4 + 1];  // G
                row[x * 3 + 2] = src[x * 4 + 0];  // R
            }
            f.write(reinterpret_cast<char*>(row.data()), rowSize);
        }
        return true;
    }

    void ApplyLights(Shader* sh, const std::vector<GpuLight>& lights) {
        sh->SetInt("uNumLights", (int)lights.size());
        for (size_t i = 0; i < lights.size(); ++i) {
            char idx[16];
            std::snprintf(idx, sizeof(idx), "[%zu]", i);
            std::string s(idx);
            sh->SetInt  ("uLightType"     + s, lights[i].type);
            sh->SetVec3 ("uLightPos"      + s, lights[i].pos);
            sh->SetVec3 ("uLightDir"      + s, lights[i].dir);
            sh->SetVec3 ("uLightColor"    + s, lights[i].color);
            sh->SetFloat("uLightRange"    + s, lights[i].range);
            sh->SetFloat("uLightInnerCos" + s, lights[i].innerCos);
            sh->SetFloat("uLightOuterCos" + s, lights[i].outerCos);
        }
    }
}

SceneRenderer::SceneRenderer() {
    m_framebuffer = new Framebuffer(64, 64);          // LDR output
    m_hdrFB       = new Framebuffer(64, 64, true);    // HDR scene buffer
    m_ldrFB       = new Framebuffer(64, 64);          // tonemapped LDR → FXAA input
    m_postShader   = new Shader(kPostVertSrc, kPostFragSrc);
    m_brightShader = new Shader(kPostVertSrc, kBrightFragSrc);
    m_blurShader   = new Shader(kPostVertSrc, kBlurFragSrc);
    m_fxaaShader   = new Shader(kPostVertSrc, kFxaaFragSrc);
    m_geomShader     = new Shader(kGeomVertSrc, kGeomFragSrc);
    m_ssaoShader     = new Shader(kPostVertSrc, kSsaoFragSrc);
    m_ssaoBlurShader = new Shader(kPostVertSrc, kSsaoBlurFragSrc);
    m_ssrShader      = new Shader(kPostVertSrc, kSsrFragSrc);

    // SSAO hemisphere kernel (64 samples, denser near origin).
    {
        std::mt19937 rng(1337);
        std::uniform_real_distribution<float> d01(0.0f, 1.0f);
        std::uniform_real_distribution<float> dN(-1.0f, 1.0f);
        for (int i = 0; i < 64; ++i) {
            glm::vec3 s(dN(rng), dN(rng), d01(rng));   // hemisphere (+z)
            s = glm::normalize(s) * d01(rng);
            float scale = (float)i / 64.0f;
            scale = 0.1f + 0.9f * scale * scale;        // bias toward center
            m_ssaoKernel.push_back(s * scale);
        }
        // 4x4 rotation-noise texture.
        std::vector<glm::vec3> noise;
        for (int i = 0; i < 16; ++i) noise.emplace_back(dN(rng), dN(rng), 0.0f);
        glGenTextures(1, &m_ssaoNoiseTex);
        glBindTexture(GL_TEXTURE_2D, m_ssaoNoiseTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 4, 4, 0, GL_RGB, GL_FLOAT, noise.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // Fullscreen quad for the post pass.
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
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);

    std::string fragSrc = std::string(kFragHeader) + kLightingGLSL + kFragMain;
    m_shader = new Shader(kVertexShaderSrc, fragSrc);
    m_skinnedShader = new Shader(kSkinnedVertexShaderSrc, fragSrc);

    m_particleShader = new Shader(kParticleVertSrc, kParticleFragSrc);
    glGenVertexArrays(1, &m_particleVao);
    glGenBuffers(1, &m_particleVbo);
    glBindVertexArray(m_particleVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_particleVbo);
    glEnableVertexAttribArray(0);   // pos
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);   // uv
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);   // color
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(5 * sizeof(float)));
    glBindVertexArray(0);
    m_lineShader = new Shader(kLineVertexShaderSrc, kLineFragmentShaderSrc);
    m_outlineShader = new Shader(kOutlineVertexShaderSrc, kOutlineFragmentShaderSrc);
    m_depthShader = new Shader(kDepthVertSrc, kDepthFragSrc);
    m_cube = new CubeMesh();
    m_sphere = new SphereMesh();
    m_plane = new PlaneMesh();
    m_grid = new Grid();
    m_env = new EnvironmentMap();
    m_env->LoadProcedural();

    // Depth-only shadow map (sampled outside the light frustum returns 1.0 = lit).
    glGenFramebuffers(1, &m_shadowFBO);
    glGenTextures(1, &m_shadowTex);
    glBindTexture(GL_TEXTURE_2D, m_shadowTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_shadowSize, m_shadowSize, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const float border[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_shadowTex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Point-light cube shadow map (stores normalized distance in R32F per face).
    m_pointDepthShader = new Shader(kPointDepthVertSrc, kPointDepthFragSrc);
    glGenTextures(1, &m_pointCubeTex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_pointCubeTex);
    for (int f = 0; f < 6; ++f)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, GL_R32F,
                     m_pointSize, m_pointSize, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glGenRenderbuffers(1, &m_pointDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_pointDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, m_pointSize, m_pointSize);

    glGenFramebuffers(1, &m_pointFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_pointFBO);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_pointDepthRBO);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
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

void SceneRenderer::SetEnvironmentHDR(const std::string& hdrPath) {
    if (m_env) m_env->LoadHDR(hdrPath);
}

bool SceneRenderer::SaveScreenshot(const std::string& path) {
    int w = m_framebuffer->GetWidth();
    int h = m_framebuffer->GetHeight();
    if (w <= 0 || h <= 0) return false;

    std::vector<unsigned char> px((size_t)w * h * 4);
    glBindTexture(GL_TEXTURE_2D, m_framebuffer->GetColorTexture());
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    return WriteBMP(path, w, h, px.data());
}

SceneRenderer::~SceneRenderer() {
    for (auto& [path, mesh]  : m_modelCache)  delete mesh;
    for (auto& [path, entry] : m_nodeShaders) delete entry.shader;
    delete m_env;
    if (m_shadowTex)     glDeleteTextures(1, &m_shadowTex);
    if (m_shadowFBO)     glDeleteFramebuffers(1, &m_shadowFBO);
    if (m_pointCubeTex)  glDeleteTextures(1, &m_pointCubeTex);
    if (m_pointDepthRBO) glDeleteRenderbuffers(1, &m_pointDepthRBO);
    if (m_pointFBO)      glDeleteFramebuffers(1, &m_pointFBO);
    delete m_pointDepthShader;
    delete m_depthShader;
    delete m_grid;
    delete m_plane;
    delete m_sphere;
    delete m_cube;
    delete m_outlineShader;
    delete m_lineShader;
    delete m_shader;
    delete m_postShader;
    delete m_brightShader;
    delete m_blurShader;
    delete m_fxaaShader;
    delete m_geomShader;
    delete m_ssaoShader;
    delete m_ssaoBlurShader;
    delete m_ssrShader;
    if (m_ssrTex)       glDeleteTextures(1, &m_ssrTex);
    if (m_ssrFBO)       glDeleteFramebuffers(1, &m_ssrFBO);
    if (m_gNormalTex)   glDeleteTextures(1, &m_gNormalTex);
    if (m_gPosTex)      glDeleteTextures(1, &m_gPosTex);
    if (m_ssaoTex)      glDeleteTextures(1, &m_ssaoTex);
    if (m_ssaoBlurTex)  glDeleteTextures(1, &m_ssaoBlurTex);
    if (m_ssaoNoiseTex) glDeleteTextures(1, &m_ssaoNoiseTex);
    if (m_gDepthRBO)    glDeleteRenderbuffers(1, &m_gDepthRBO);
    if (m_gFBO)         glDeleteFramebuffers(1, &m_gFBO);
    if (m_ssaoFBO)      glDeleteFramebuffers(1, &m_ssaoFBO);
    if (m_ssaoBlurFBO)  glDeleteFramebuffers(1, &m_ssaoBlurFBO);
    if (m_bloomTex[0]) glDeleteTextures(2, m_bloomTex);
    if (m_bloomFBO[0]) glDeleteFramebuffers(2, m_bloomFBO);
    if (m_quadVBO) glDeleteBuffers(1, &m_quadVBO);
    if (m_quadVAO) glDeleteVertexArrays(1, &m_quadVAO);
    delete m_ldrFB;
    delete m_hdrFB;
    delete m_framebuffer;
}

std::vector<std::string> SceneRenderer::ModelAnimNames(const std::string& emdlPath) {
    std::vector<std::string> names;
    ModelMesh* m = GetOrLoadModel(emdlPath);
    if (m)
        for (const AnimClip& c : m->Anims())
            names.push_back(c.name.empty() ? std::string("clip") : c.name);
    return names;
}

void SceneRenderer::UpdateAndRenderParticles(Scene& scene, const glm::mat4& view, const glm::mat4& projection) {
    entt::registry& reg = scene.Reg();

    double now = NowSeconds();
    float dt = (m_lastParticleTime > 0.0) ? static_cast<float>(now - m_lastParticleTime) : 0.0f;
    m_lastParticleTime = now;
    if (dt < 0.0f)  dt = 0.0f;
    if (dt > 0.1f)  dt = 0.1f;   // clamp hitches

    static std::mt19937 rng(1337u);
    auto rnd = [&](float a, float b) { std::uniform_real_distribution<float> d(a, b); return d(rng); };

    // Drop pools whose emitter is gone.
    for (auto it = m_particlePools.begin(); it != m_particlePools.end();) {
        if (!reg.valid(it->first) || !reg.all_of<ParticleEmitterComponent>(it->first))
            it = m_particlePools.erase(it);
        else ++it;
    }

    // Simulate + emit.
    for (auto [e, em] : reg.view<ParticleEmitterComponent>().each()) {
        ParticlePool& pool = m_particlePools[e];
        glm::vec3 origin = glm::vec3(WorldMatrixOf(reg, e)[3]);

        for (size_t i = 0; i < pool.particles.size();) {
            Particle& p = pool.particles[i];
            p.life -= dt;
            if (p.life <= 0.0f) { pool.particles[i] = pool.particles.back(); pool.particles.pop_back(); continue; }
            p.vel += em.gravity * dt;
            p.pos += p.vel * dt;
            ++i;
        }

        int cap = em.maxParticles > 0 ? em.maxParticles : 0;
        if (em.emitting) pool.emitAccum += em.rate * dt;
        if (pool.emitAccum > 10000.0f) pool.emitAccum = 0.0f;   // safety
        while (em.emitting && pool.emitAccum >= 1.0f && static_cast<int>(pool.particles.size()) < cap) {
            pool.emitAccum -= 1.0f;
            glm::vec3 base = (glm::length(em.direction) > 1e-5f) ? glm::normalize(em.direction) : glm::vec3(0, 1, 0);
            glm::vec3 dir  = glm::normalize(base + glm::vec3(rnd(-1, 1), rnd(-1, 1), rnd(-1, 1)) * em.spread);
            Particle p;
            p.pos     = origin;
            p.vel     = dir * em.speed;
            p.maxLife = std::max(0.05f, em.lifetime * (1.0f + rnd(-em.lifetimeVar, em.lifetimeVar)));
            p.life    = p.maxLife;
            p.size0   = em.startSize;  p.size1 = em.endSize;
            p.col0    = em.startColor; p.col1  = em.endColor;
            pool.particles.push_back(p);
        }
    }

    bool any = false;
    for (auto& kv : m_particlePools) if (!kv.second.particles.empty()) { any = true; break; }
    if (!any) return;

    // Billboard basis from the view rotation rows.
    glm::vec3 camRight(view[0][0], view[1][0], view[2][0]);
    glm::vec3 camUp   (view[0][1], view[1][1], view[2][1]);

    m_particleShader->Bind();
    m_particleShader->SetMat4("uView", view);
    m_particleShader->SetMat4("uProjection", projection);
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);   // test against scene depth, don't write

    glBindVertexArray(m_particleVao);
    std::vector<float> verts;
    for (auto [e, em] : reg.view<ParticleEmitterComponent>().each()) {
        auto pit = m_particlePools.find(e);
        if (pit == m_particlePools.end() || pit->second.particles.empty()) continue;
        const ParticlePool& pool = pit->second;

        if (em.blend == ParticleBlend::Additive) glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        else                                     glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        int hasTex = 0;
        if (!em.texture.empty()) {
            Texture* t = TextureManager::GetOrLoad(em.texture);
            if (t) { t->Bind(0); m_particleShader->SetInt("uTex", 0); hasTex = 1; }
        }
        m_particleShader->SetInt("uHasTex", hasTex);
        if (!hasTex) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, 0); }

        verts.clear();
        verts.reserve(pool.particles.size() * 6 * 9);
        for (const Particle& p : pool.particles) {
            float t    = (p.maxLife > 0.0f) ? 1.0f - p.life / p.maxLife : 1.0f;
            float half = glm::mix(p.size0, p.size1, t) * 0.5f;
            glm::vec4 col = glm::mix(p.col0, p.col1, t);
            glm::vec3 r = camRight * half, u = camUp * half, c = p.pos;
            glm::vec3 bl = c - r - u, br = c + r - u, tr = c + r + u, tl = c - r + u;
            auto push = [&](const glm::vec3& q, float uu, float vv) {
                verts.insert(verts.end(), { q.x, q.y, q.z, uu, vv, col.r, col.g, col.b, col.a });
            };
            push(bl, 0, 0); push(br, 1, 0); push(tr, 1, 1);
            push(bl, 0, 0); push(tr, 1, 1); push(tl, 0, 1);
        }
        glBindBuffer(GL_ARRAY_BUFFER, m_particleVbo);
        glBufferData(GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(verts.size() * sizeof(float)), verts.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(pool.particles.size() * 6));
    }
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);   // restore default blend
    m_particleShader->Unbind();
}

ModelMesh* SceneRenderer::GetOrLoadModel(const std::string& emdlPath) {
    if (emdlPath.empty()) return nullptr;
    auto it = m_modelCache.find(emdlPath);
    if (it != m_modelCache.end()) return it->second;
    ModelMesh* mesh = ModelMesh::LoadFromEmdl(emdlPath);
    m_modelCache[emdlPath] = mesh;
    return mesh;
}

PrimitiveMesh* SceneRenderer::GetMeshForKind(MeshKind kind) const {
    switch (kind) {
    case MeshKind::Cube:   return m_cube;
    case MeshKind::Sphere: return m_sphere;
    case MeshKind::Plane:  return m_plane;
    case MeshKind::Model:
    case MeshKind::None:
    default:
        return nullptr;
    }
}

void SceneRenderer::RenderShadowPass(Scene& scene) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFBO);
    glViewport(0, 0, m_shadowSize, m_shadowSize);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    // Render back faces only — the occluder's thickness provides the depth
    // offset, killing self-shadow acne (the light-angle shimmer) on lit faces.
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    m_depthShader->Bind();
    m_depthShader->SetMat4("uLightSpace", m_lightSpace);

    auto view = scene.Reg().view<TransformComponent, MeshComponent>();
    for (auto e : view) {
        const MeshComponent& mc = view.get<MeshComponent>(e);
        PrimitiveMesh* primMesh  = nullptr;
        ModelMesh*     modelMesh = nullptr;
        if (mc.kind == MeshKind::Model) modelMesh = GetOrLoadModel(mc.modelPath);
        else                            primMesh  = GetMeshForKind(mc.kind);
        if (!primMesh && !modelMesh) continue;

        m_depthShader->SetMat4("uModel", WorldMatrixOf(scene.Reg(), e));
        if (modelMesh) modelMesh->Draw();
        else           primMesh->Draw();
    }

    m_depthShader->Unbind();
    glCullFace(GL_BACK);
    glDisable(GL_CULL_FACE);            // restore default (no culling)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SceneRenderer::RenderPointShadowPass(Scene& scene,
                                          const glm::vec3& lightPos, float farPlane) {
    static const glm::vec3 dirs[6] = {
        { 1, 0, 0}, {-1, 0, 0}, { 0, 1, 0}, { 0,-1, 0}, { 0, 0, 1}, { 0, 0,-1} };
    static const glm::vec3 ups[6]  = {
        { 0,-1, 0}, { 0,-1, 0}, { 0, 0, 1}, { 0, 0,-1}, { 0,-1, 0}, { 0,-1, 0} };

    glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.05f, glm::max(farPlane, 1.0f));

    glBindFramebuffer(GL_FRAMEBUFFER, m_pointFBO);
    glViewport(0, 0, m_pointSize, m_pointSize);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    // Render back faces only: the occluder's own thickness provides the depth
    // offset, so shadows hug silhouette edges without peter-panning.
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    m_pointDepthShader->Bind();
    m_pointDepthShader->SetVec3 ("uLightPos", lightPos);
    m_pointDepthShader->SetFloat("uFarPlane", glm::max(farPlane, 1.0f));

    for (int f = 0; f < 6; ++f) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, m_pointCubeTex, 0);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);            // 1.0 = far/unshadowed
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 viewProj = proj * glm::lookAt(lightPos, lightPos + dirs[f], ups[f]);
        m_pointDepthShader->SetMat4("uViewProj", viewProj);

        auto view = scene.Reg().view<TransformComponent, MeshComponent>();
        for (auto e : view) {
            const MeshComponent& mc = view.get<MeshComponent>(e);
            PrimitiveMesh* primMesh  = nullptr;
            ModelMesh*     modelMesh = nullptr;
            if (mc.kind == MeshKind::Model) modelMesh = GetOrLoadModel(mc.modelPath);
            else                            primMesh  = GetMeshForKind(mc.kind);
            if (!primMesh && !modelMesh) continue;

            m_pointDepthShader->SetMat4("uModel", WorldMatrixOf(scene.Reg(), e));
            if (modelMesh) modelMesh->Draw();
            else           primMesh->Draw();
        }
    }

    m_pointDepthShader->Unbind();
    glCullFace(GL_BACK);
    glDisable(GL_CULL_FACE);              // restore default (no culling)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SceneRenderer::EnsureBloomTargets(int width, int height) {
    int bw = std::max(1, width / 2);
    int bh = std::max(1, height / 2);
    if (bw == m_bloomW && bh == m_bloomH && m_bloomFBO[0]) return;
    m_bloomW = bw; m_bloomH = bh;

    for (int i = 0; i < 2; ++i) {
        if (!m_bloomFBO[i]) glGenFramebuffers(1, &m_bloomFBO[i]);
        if (!m_bloomTex[i]) glGenTextures(1, &m_bloomTex[i]);
        glBindTexture(GL_TEXTURE_2D, m_bloomTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, bw, bh, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFBO[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_bloomTex[i], 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SceneRenderer::RenderBloom() {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glViewport(0, 0, m_bloomW, m_bloomH);
    glBindVertexArray(m_quadVAO);

    // Bright-pass: HDR scene → bloomTex[0].
    glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFBO[0]);
    glClear(GL_COLOR_BUFFER_BIT);
    m_brightShader->Bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_hdrFB->GetColorTexture());
    m_brightShader->SetInt("uScene", 0);
    m_brightShader->SetFloat("uThreshold", m_bloomThreshold);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    m_brightShader->Unbind();

    // Separable Gaussian blur, ping-pong. Result ends up in bloomTex[0].
    m_blurShader->Bind();
    m_blurShader->SetInt("uImage", 0);
    bool horizontal = true;
    const int passes = 10;
    for (int i = 0; i < passes; ++i) {
        int dst = horizontal ? 1 : 0;
        int src = horizontal ? 0 : 1;
        glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFBO[dst]);
        glClear(GL_COLOR_BUFFER_BIT);
        m_blurShader->SetInt("uHorizontal", horizontal ? 1 : 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_bloomTex[src]);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        horizontal = !horizontal;
    }
    m_blurShader->Unbind();

    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SceneRenderer::EnsureGBuffer(int width, int height) {
    if (width == m_gW && height == m_gH && m_gFBO) return;
    m_gW = width; m_gH = height;

    auto mkColor = [&](uint32_t& tex, GLint internal, GLenum fmt, GLenum type, GLint filter) {
        if (!tex) glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, internal, width, height, 0, fmt, type, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    };

    // G-buffer: view-space normal + position, plus a depth renderbuffer.
    if (!m_gFBO) glGenFramebuffers(1, &m_gFBO);
    mkColor(m_gNormalTex, GL_RGBA16F, GL_RGBA, GL_FLOAT, GL_NEAREST);
    mkColor(m_gPosTex,    GL_RGBA16F, GL_RGBA, GL_FLOAT, GL_NEAREST);
    if (!m_gDepthRBO) glGenRenderbuffers(1, &m_gDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_gDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, m_gFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_gNormalTex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_gPosTex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_gDepthRBO);
    { GLenum bufs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 }; glDrawBuffers(2, bufs); }

    // SSAO target + blurred result (single channel).
    if (!m_ssaoFBO) glGenFramebuffers(1, &m_ssaoFBO);
    mkColor(m_ssaoTex, GL_R16F, GL_RED, GL_FLOAT, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, m_ssaoFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ssaoTex, 0);

    if (!m_ssaoBlurFBO) glGenFramebuffers(1, &m_ssaoBlurFBO);
    mkColor(m_ssaoBlurTex, GL_R16F, GL_RED, GL_FLOAT, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, m_ssaoBlurFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ssaoBlurTex, 0);

    // SSR reflection target (HDR).
    if (!m_ssrFBO) glGenFramebuffers(1, &m_ssrFBO);
    mkColor(m_ssrTex, GL_RGBA16F, GL_RGBA, GL_FLOAT, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, m_ssrFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ssrTex, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

void SceneRenderer::RenderGBuffer(Scene& scene,
                                  const glm::mat4& view, const glm::mat4& projection) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_gFBO);
    glViewport(0, 0, m_gW, m_gH);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_geomShader->Bind();
    m_geomShader->SetMat4("uView", view);
    m_geomShader->SetMat4("uProj", projection);
    auto gview = scene.Reg().view<TransformComponent, MeshComponent>();
    for (auto e : gview) {
        const MeshComponent& mc = gview.get<MeshComponent>(e);
        PrimitiveMesh* primMesh  = nullptr;
        ModelMesh*     modelMesh = nullptr;
        if (mc.kind == MeshKind::Model) modelMesh = GetOrLoadModel(mc.modelPath);
        else                            primMesh  = GetMeshForKind(mc.kind);
        if (!primMesh && !modelMesh) continue;
        m_geomShader->SetMat4("uModel", WorldMatrixOf(scene.Reg(), e));
        if (modelMesh) modelMesh->Draw();
        else           primMesh->Draw();
    }
    m_geomShader->Unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SceneRenderer::RenderSSAO(const glm::mat4& projection) {
    // Occlusion pass.
    glBindFramebuffer(GL_FRAMEBUFFER, m_ssaoFBO);
    glViewport(0, 0, m_gW, m_gH);
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT);
    m_ssaoShader->Bind();
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_gNormalTex);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, m_gPosTex);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, m_ssaoNoiseTex);
    m_ssaoShader->SetInt("gNormal",   0);
    m_ssaoShader->SetInt("gPosition", 1);
    m_ssaoShader->SetInt("uNoise",    2);
    m_ssaoShader->SetMat4("uProj", projection);
    m_ssaoShader->SetVec2("uNoiseScale", glm::vec2(m_gW / 4.0f, m_gH / 4.0f));
    m_ssaoShader->SetFloat("uRadius", m_ssaoRadius);
    m_ssaoShader->SetFloat("uBias",   m_ssaoBias);
    m_ssaoShader->SetFloat("uPower",  m_ssaoPower);
    for (size_t i = 0; i < m_ssaoKernel.size(); ++i) {
        char nm[24]; std::snprintf(nm, sizeof(nm), "uKernel[%zu]", i);
        m_ssaoShader->SetVec3(nm, m_ssaoKernel[i]);
    }
    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // Blur pass.
    glBindFramebuffer(GL_FRAMEBUFFER, m_ssaoBlurFBO);
    glClear(GL_COLOR_BUFFER_BIT);
    m_ssaoBlurShader->Bind();
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_ssaoTex);
    m_ssaoBlurShader->SetInt("uSSAOInput", 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    m_ssaoBlurShader->Unbind();

    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SceneRenderer::RenderSSR(const glm::mat4& projection) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_ssrFBO);
    glViewport(0, 0, m_gW, m_gH);
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT);
    m_ssrShader->Bind();
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_gPosTex);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, m_gNormalTex);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, m_hdrFB->GetColorTexture());
    m_ssrShader->SetInt("gPosition", 0);
    m_ssrShader->SetInt("gNormal",   1);
    m_ssrShader->SetInt("uScene",    2);
    m_ssrShader->SetMat4("uProj", projection);
    m_ssrShader->SetFloat("uMaxDistance", m_ssrMaxDist);
    m_ssrShader->SetInt("uSteps", m_ssrSteps);
    m_ssrShader->SetFloat("uThickness", m_ssrThickness);
    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    m_ssrShader->Unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

namespace {
    glm::vec3 SampleVec3(const std::vector<std::pair<float, glm::vec3>>& keys, float t, const glm::vec3& fallback) {
        if (keys.empty()) return fallback;
        if (keys.size() == 1 || t <= keys.front().first) return keys.front().second;
        if (t >= keys.back().first) return keys.back().second;
        for (size_t i = 0; i + 1 < keys.size(); ++i)
            if (t < keys[i + 1].first) {
                float span = keys[i + 1].first - keys[i].first;
                float f = span > 0.0f ? (t - keys[i].first) / span : 0.0f;
                return glm::mix(keys[i].second, keys[i + 1].second, f);
            }
        return keys.back().second;
    }
    glm::quat SampleQuat(const std::vector<std::pair<float, glm::quat>>& keys, float t) {
        if (keys.empty()) return glm::quat(1, 0, 0, 0);
        if (keys.size() == 1 || t <= keys.front().first) return keys.front().second;
        if (t >= keys.back().first) return keys.back().second;
        for (size_t i = 0; i + 1 < keys.size(); ++i)
            if (t < keys[i + 1].first) {
                float span = keys[i + 1].first - keys[i].first;
                float f = span > 0.0f ? (t - keys[i].first) / span : 0.0f;
                return glm::normalize(glm::slerp(keys[i].second, keys[i + 1].second, f));
            }
        return keys.back().second;
    }

    // Sample a clip at `seconds` and produce the per-bone skinning palette.
    void SampleAnimation(const ModelMesh& mesh, int clipIdx, float seconds, bool loop,
                         std::vector<glm::mat4>& out) {
        const std::vector<ModelBone>& bones = mesh.Bones();
        const AnimClip& clip = mesh.Anims()[clipIdx];
        float tps   = clip.tps > 0.0f ? clip.tps : 25.0f;
        float dur   = clip.duration > 0.0f ? clip.duration : 1.0f;
        float ticks = seconds * tps;
        float t = loop ? std::fmod(ticks, dur) : std::min(ticks, dur);
        if (t < 0.0f) t = 0.0f;

        std::vector<glm::mat4> localT(bones.size());
        for (size_t i = 0; i < bones.size(); ++i) localT[i] = bones[i].localBind;
        for (const AnimChannel& ch : clip.channels) {
            if (ch.bone < 0 || ch.bone >= static_cast<int>(bones.size())) continue;
            glm::vec3 p = SampleVec3(ch.pos, t, glm::vec3(bones[ch.bone].localBind[3]));
            glm::quat r = SampleQuat(ch.rot, t);
            glm::vec3 s = SampleVec3(ch.scl, t, glm::vec3(1.0f));
            localT[ch.bone] = glm::translate(glm::mat4(1.0f), p) * glm::mat4_cast(r) * glm::scale(glm::mat4(1.0f), s);
        }

        std::vector<glm::mat4> global(bones.size());
        out.resize(bones.size());
        for (size_t i = 0; i < bones.size(); ++i) {
            global[i] = (bones[i].parent < 0) ? localT[i] : global[bones[i].parent] * localT[i];
            out[i] = mesh.GlobalInverse() * global[i] * bones[i].offset;
        }
    }

    std::vector<glm::mat4> g_boneScratch;   // reused each call; consumed immediately

    // The bone-matrix palette for a skinned mesh: the entity's animated pose if it
    // has an Animator (with a valid clip), otherwise the rest/bind pose.
    const std::vector<glm::mat4>& BoneMatricesFor(entt::registry& reg, entt::entity e, ModelMesh* mesh) {
        const AnimatorComponent* anim = reg.try_get<AnimatorComponent>(e);
        if (anim && !mesh->Anims().empty() && anim->clip >= 0 &&
            anim->clip < static_cast<int>(mesh->Anims().size())) {
            SampleAnimation(*mesh, anim->clip, anim->time, anim->loop, g_boneScratch);
            return g_boneScratch;
        }
        return mesh->BindPose();
    }
}

uint32_t SceneRenderer::Render(Scene& scene, entt::entity selected,
    int width, int height,
    const glm::mat4& view, const glm::mat4& projection) {
    if (width <= 0 || height <= 0) return 0;
    entt::registry& reg = scene.Reg();

    // Refresh world matrices from the parent hierarchy before any pass reads them.
    UpdateWorldTransforms(reg);

    // Gather scene lights once per frame; applied to every lit shader.
    std::vector<GpuLight> lights = CollectLights(scene);

    // Pick a single shadow caster: prefer the first directional, else first spot.
    int shadowLight = -1;
    for (size_t i = 0; i < lights.size(); ++i)
        if (lights[i].type == 0) { shadowLight = (int)i; break; }
    if (shadowLight < 0)
        for (size_t i = 0; i < lights.size(); ++i)
            if (lights[i].type == 2) { shadowLight = (int)i; break; }

    // Build the light-space matrix and render the shadow depth map.
    if (shadowLight >= 0) {
        const GpuLight& sl = lights[shadowLight];

        // Fit a bounding sphere around all shadow-casting geometry (world AABBs).
        glm::vec3 bmin( 1e9f), bmax(-1e9f);
        bool any = false;
        auto mview = reg.view<TransformComponent, MeshComponent>();
        for (auto e : mview) {
            const MeshComponent& mc = mview.get<MeshComponent>(e);
            glm::mat4 m = WorldMatrixOf(reg, e);
            for (int c = 0; c < 8; ++c) {
                glm::vec4 corner(
                    (c & 1) ? mc.aabbMax.x : mc.aabbMin.x,
                    (c & 2) ? mc.aabbMax.y : mc.aabbMin.y,
                    (c & 4) ? mc.aabbMax.z : mc.aabbMin.z, 1.0f);
                glm::vec3 w = glm::vec3(m * corner);
                bmin = glm::min(bmin, w);
                bmax = glm::max(bmax, w);
                any = true;
            }
        }
        glm::vec3 center = any ? (bmin + bmax) * 0.5f : glm::vec3(0.0f);
        float     radius = any ? glm::max(glm::length(bmax - bmin) * 0.5f, 1.0f) : 10.0f;

        glm::vec3 up = (fabsf(sl.dir.y) > 0.99f) ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
        if (sl.type == 2) {                                   // spot → perspective
            glm::mat4 lview = glm::lookAt(sl.pos, sl.pos + sl.dir, up);
            float fov = 2.0f * acosf(glm::clamp(sl.outerCos, -1.0f, 1.0f)) * 1.1f;
            fov = glm::clamp(fov, glm::radians(5.0f), glm::radians(170.0f));
            glm::mat4 lproj = glm::perspective(fov, 1.0f, 0.05f, glm::max(sl.range, 1.0f));
            m_lightSpace = lproj * lview;
        } else {                                              // directional → ortho
            glm::vec3 eye = center - sl.dir * (radius + 5.0f);
            glm::mat4 lview = glm::lookAt(eye, center, up);
            glm::mat4 lproj = glm::ortho(-radius, radius, -radius, radius,
                                         0.1f, 2.0f * radius + 10.0f);
            m_lightSpace = lproj * lview;
        }
        RenderShadowPass(scene);
    }

    // Point-light shadow caster: first point light gets a cube shadow map.
    int   pointShadow    = -1;
    glm::vec3 pointPos(0.0f);
    float pointFar       = 1.0f;
    for (size_t i = 0; i < lights.size(); ++i)
        if (lights[i].type == 1) { pointShadow = (int)i; break; }
    if (pointShadow >= 0) {
        pointPos = lights[pointShadow].pos;
        pointFar = glm::max(lights[pointShadow].range, 1.0f);
        RenderPointShadowPass(scene, pointPos, pointFar);
    }

    // ── View-space G-buffer prepass (shared by SSAO + SSR) ──────────────
    if (m_ssaoEnabled || m_ssrEnabled) {
        EnsureGBuffer(width, height);
        RenderGBuffer(scene, view, projection);
        if (m_ssaoEnabled) RenderSSAO(projection);
    }

    m_framebuffer->Resize(width, height);   // LDR output target (post result)
    m_ldrFB->Resize(width, height);
    m_hdrFB->Resize(width, height);
    m_hdrFB->Bind();                        // scene renders into HDR buffer

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

    // Shadow maps: 2D (dir/spot) on unit 7, point cube on unit 6.
    constexpr int kShadowUnit = 7;
    constexpr int kPointUnit  = 6;
    glActiveTexture(GL_TEXTURE0 + kShadowUnit);
    glBindTexture(GL_TEXTURE_2D, m_shadowTex);
    glActiveTexture(GL_TEXTURE0 + kPointUnit);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_pointCubeTex);

    // IBL maps: irradiance (8), prefilter (9), BRDF LUT (10).
    constexpr int kIrrUnit  = 8;
    constexpr int kPrefUnit = 9;
    constexpr int kBrdfUnit = 10;
    const int hasIBL = (m_env && m_env->HasIBL()) ? 1 : 0;
    if (hasIBL) {
        glActiveTexture(GL_TEXTURE0 + kIrrUnit);  glBindTexture(GL_TEXTURE_CUBE_MAP, m_env->GetIrradiance());
        glActiveTexture(GL_TEXTURE0 + kPrefUnit); glBindTexture(GL_TEXTURE_CUBE_MAP, m_env->GetPrefilter());
        glActiveTexture(GL_TEXTURE0 + kBrdfUnit); glBindTexture(GL_TEXTURE_2D,       m_env->GetBrdfLUT());
    }

    // SSAO occlusion texture on unit 11.
    constexpr int kSSAOUnit = 11;
    const int hasSSAO = m_ssaoEnabled ? 1 : 0;
    if (hasSSAO) {
        glActiveTexture(GL_TEXTURE0 + kSSAOUnit);
        glBindTexture(GL_TEXTURE_2D, m_ssaoBlurTex);
    }
    glm::vec2 screenSize((float)width, (float)height);

    // Frame-constant uniforms, applied to both the lit shader and its skinned
    // variant (each is bound per-mesh in the loop below).
    auto setupLit = [&](Shader* s) {
        s->Bind();
        s->SetMat4("uView",       view);
        s->SetMat4("uProjection", projection);
        s->SetVec3("uCamPos",     camPos);
        s->SetInt("uAlbedoTex",     0);
        s->SetInt("uNormalTex",     1);
        s->SetInt("uMetalRoughTex", 2);
        s->SetInt("uShadowMap",     kShadowUnit);
        s->SetInt("uShadowLight",   shadowLight);
        s->SetMat4("uLightSpace",   m_lightSpace);
        s->SetInt  ("uPointShadow",      kPointUnit);
        s->SetInt  ("uPointShadowLight", pointShadow);
        s->SetVec3 ("uPointLightPos",    pointPos);
        s->SetFloat("uPointFar",         pointFar);
        s->SetInt  ("uIrradiance", kIrrUnit);
        s->SetInt  ("uPrefilter",  kPrefUnit);
        s->SetInt  ("uBrdfLUT",    kBrdfUnit);
        s->SetInt  ("uHasIBL",     hasIBL);
        s->SetInt  ("uSSAO",        kSSAOUnit);
        s->SetInt  ("uSSAOEnabled", hasSSAO);
        s->SetVec2 ("uScreenSize",  screenSize);
        ApplyLights(s, lights);
    };
    setupLit(m_shader);
    setupLit(m_skinnedShader);
    m_shader->Bind();

    auto renderView = reg.view<TransformComponent, MeshComponent>();
    for (auto e : renderView) {
        const MeshComponent& mc = renderView.get<MeshComponent>(e);

        // Resolve mesh first so hasTangents is known before setting uniforms
        PrimitiveMesh* primMesh  = nullptr;
        ModelMesh*     modelMesh = nullptr;
        if (mc.kind == MeshKind::Model)
            modelMesh = GetOrLoadModel(mc.modelPath);
        else
            primMesh = GetMeshForKind(mc.kind);

        if (!primMesh && !modelMesh) continue;

        const MaterialComponent* matc = reg.try_get<MaterialComponent>(e);
        std::string materialPath = matc ? matc->path : std::string();

        glm::vec3 color(kDefaultColor);
        float     metallic  = 0.0f;
        float     roughness = 0.5f;
        glm::vec3 emissive(0.0f);
        Texture*  albedoTex = nullptr;
        Texture*  normalTex = nullptr;
        Texture*  mrTex     = nullptr;

        if (!materialPath.empty()) {
            Material* mat = MaterialManager::GetOrLoad(materialPath);
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
        glm::mat4 model = WorldMatrixOf(reg, e);
        bool isSelected = (e == selected);

        // Stencil setup (same for both shader paths)
        if (isSelected) {
            glStencilFunc(GL_ALWAYS, 1, 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        } else {
            glStencilFunc(GL_ALWAYS, 0, 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        }

        // ── Node-compiled shader? ─────────────────────────────────────────
        auto nodeIt = !materialPath.empty()
                    ? m_nodeShaders.find(materialPath)
                    : m_nodeShaders.end();

        if (nodeIt != m_nodeShaders.end() && nodeIt->second.shader) {
            Shader* cs = nodeIt->second.shader;
            cs->Bind();
            cs->SetMat4("uModel", model);
            cs->SetMat4("uView",  view);
            cs->SetMat4("uProj",  projection);
            cs->SetVec3("uCamPos", camPos);
            cs->SetFloat("uTime", NowSeconds());
            cs->SetInt ("uShadowMap",   kShadowUnit);   // 2D shadow stays bound at unit 7
            cs->SetInt ("uShadowLight", shadowLight);
            cs->SetMat4("uLightSpace",  m_lightSpace);
            cs->SetInt  ("uPointShadow",      kPointUnit);   // cube stays bound at unit 6
            cs->SetInt  ("uPointShadowLight", pointShadow);
            cs->SetVec3 ("uPointLightPos",    pointPos);
            cs->SetFloat("uPointFar",         pointFar);
            cs->SetInt  ("uIrradiance", kIrrUnit);
            cs->SetInt  ("uPrefilter",  kPrefUnit);
            cs->SetInt  ("uBrdfLUT",    kBrdfUnit);
            cs->SetInt  ("uHasIBL",     hasIBL);
            cs->SetInt  ("uSSAO",        kSSAOUnit);
            cs->SetInt  ("uSSAOEnabled", hasSSAO);
            cs->SetVec2 ("uScreenSize",  screenSize);
            cs->SetInt  ("uTonemap",    0);   // scene is HDR; post pass tonemaps
            ApplyLights(cs, lights);

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
            // ── Default PBR shader (skinned variant for rigged models) ────
            const bool skinned = modelMesh && modelMesh->IsSkinned();
            Shader* sh = skinned ? m_skinnedShader : m_shader;
            sh->Bind();
            sh->SetVec3 ("uColor",            color);
            sh->SetFloat("uMetallic",          metallic);
            sh->SetFloat("uRoughness",         roughness);
            sh->SetVec3 ("uEmissive",          emissive);
            sh->SetInt  ("uHasAlbedoTex",      albedoTex ? 1 : 0);
            sh->SetInt  ("uHasNormalTex",       normalTex ? 1 : 0);
            sh->SetInt  ("uHasMetalRoughTex",  mrTex     ? 1 : 0);
            sh->SetInt  ("uHasTangents",        hasTangents ? 1 : 0);

            if (albedoTex) albedoTex->Bind(0);
            else { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, 0); }
            if (normalTex) normalTex->Bind(1);
            else { glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, 0); }
            if (mrTex) mrTex->Bind(2);
            else { glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, 0); }

            sh->SetMat4("uModel", model);

            if (skinned) {
                const std::vector<glm::mat4>& pose = BoneMatricesFor(reg, e, modelMesh);
                int n = std::min(static_cast<int>(pose.size()), 256);
                if (n > 0) sh->SetMat4Array("uBones", pose.data(), n);
            }

            if (modelMesh) modelMesh->Draw();
            else           primMesh->Draw();
        }
    }

    // Skybox fills the background (depth == far) after opaque geometry.
    // Mask stencil writes so it can't clobber the selection stencil (the
    // outline pass reads it next).
    glStencilMask(0x00);
    if (m_env) m_env->RenderSkybox(view, projection);
    glStencilMask(0xFF);

    if (reg.valid(selected) && reg.all_of<TransformComponent, MeshComponent>(selected)) {
        const MeshComponent& mc = reg.get<MeshComponent>(selected);

        PrimitiveMesh* primMesh  = nullptr;
        ModelMesh*     modelMesh = nullptr;
        if (mc.kind == MeshKind::Model)
            modelMesh = GetOrLoadModel(mc.modelPath);
        else
            primMesh = GetMeshForKind(mc.kind);

        if (primMesh || modelMesh) {
            glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
            glStencilMask(0x00);

            m_outlineShader->Bind();
            glm::mat4 model = WorldMatrixOf(reg, selected);
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

    // Transparent particles over the lit scene (still in the HDR buffer).
    UpdateAndRenderParticles(scene, view, projection);

    m_hdrFB->Unbind();

    // ── SSR: reflections from the G-buffer + lit HDR color ──────────────
    if (m_ssrEnabled) RenderSSR(projection);

    // ── Bloom: bright-pass + blur from the HDR scene ─────────────────────
    EnsureBloomTargets(width, height);
    if (m_bloomEnabled) RenderBloom();

    // ── Post pass: (scene + bloom) → tonemap + exposure + gamma → LDR ────
    // Tonemap into the FXAA input buffer (or straight to output if AA off).
    const bool fxaaOn = (m_aaMode == AAMode::FXAA);
    Framebuffer* tonemapTarget = fxaaOn ? m_ldrFB : m_framebuffer;
    tonemapTarget->Bind();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glClear(GL_COLOR_BUFFER_BIT);
    m_postShader->Bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_hdrFB->GetColorTexture());
    m_postShader->SetInt("uScene", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_bloomEnabled ? m_bloomTex[0] : 0);
    m_postShader->SetInt("uBloom", 1);
    m_postShader->SetInt("uHasBloom", m_bloomEnabled ? 1 : 0);
    m_postShader->SetFloat("uBloomIntensity", m_bloomIntensity);
    m_postShader->SetFloat("uExposure", m_exposure);
    m_postShader->SetFloat("uContrast",    m_contrast);
    m_postShader->SetFloat("uSaturation",  m_saturation);
    m_postShader->SetFloat("uTemperature", m_temperature);
    m_postShader->SetFloat("uVignette",    m_vignette);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_ssrEnabled ? m_ssrTex : 0);
    m_postShader->SetInt("uSSR", 2);
    m_postShader->SetInt("uSSREnabled", m_ssrEnabled ? 1 : 0);
    m_postShader->SetFloat("uSSRIntensity", m_ssrIntensity);
    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    m_postShader->Unbind();

    // ── FXAA pass: tonemapped LDR → final output ─────────────────────────
    if (fxaaOn) {
        m_framebuffer->Bind();
        glClear(GL_COLOR_BUFFER_BIT);
        m_fxaaShader->Bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_ldrFB->GetColorTexture());
        m_fxaaShader->SetInt("uTex", 0);
        m_fxaaShader->SetVec2("uRcpFrame", glm::vec2(1.0f / width, 1.0f / height));
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        m_fxaaShader->Unbind();
        m_framebuffer->Unbind();
    } else {
        tonemapTarget->Unbind();
    }

    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);

    return m_framebuffer->GetColorTexture();
}