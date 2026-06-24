#pragma once

// Shared PBR lighting GLSL injected into both the default scene shader and the
// node-compiled material shaders. Assumes the including shader already defines:
//   - const float PI
//   - float D_GGX(float NdotH, float r)
//   - float G_Smith(float NdotV, float NdotL, float r)
//   - vec3  F_Schlick(float cosTheta, vec3 F0)
//   - in    vec3 vWorldPos
// Insert this block AFTER those definitions and BEFORE main().
//
// Light directions follow the convention: uLightDir is the direction the light
// travels (its forward). uLightColor already includes intensity.
inline const char* kLightingGLSL = R"GLSL(
#define MAX_LIGHTS 8
uniform int   uNumLights;
uniform int   uLightType   [MAX_LIGHTS];   // 0=dir, 1=point, 2=spot
uniform vec3  uLightPos     [MAX_LIGHTS];
uniform vec3  uLightDir     [MAX_LIGHTS];
uniform vec3  uLightColor    [MAX_LIGHTS];  // color * intensity
uniform float uLightRange    [MAX_LIGHTS];
uniform float uLightInnerCos [MAX_LIGHTS];
uniform float uLightOuterCos [MAX_LIGHTS];

// Single-caster shadow map (directional or spot). uShadowLight = light index
// that casts shadows, or -1 when no shadows this frame.
uniform sampler2D uShadowMap;
uniform mat4      uLightSpace;
uniform int       uShadowLight;

float shadowFactor(vec3 N, vec3 L) {
    vec4 lp = uLightSpace * vec4(vWorldPos, 1.0);
    vec3 p  = lp.xyz / lp.w;
    p = p * 0.5 + 0.5;                              // NDC → [0,1]
    if (p.z > 1.0) return 0.0;                      // beyond far plane → lit
    float bias = max(0.0012 * (1.0 - dot(N, L)), 0.0004);  // small: back-face depth offsets
    float cur  = p.z - bias;
    vec2  texel = 1.0 / vec2(textureSize(uShadowMap, 0));
    float sh = 0.0;                                 // 3x3 PCF
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
            sh += cur > texture(uShadowMap, p.xy + vec2(x, y) * texel).r ? 1.0 : 0.0;
    return sh / 9.0;
}

// Omnidirectional (point-light) shadows via a distance cube map.
uniform samplerCube uPointShadow;
uniform int         uPointShadowLight;   // light index, -1 = none
uniform vec3        uPointLightPos;
uniform float       uPointFar;

float pointShadowFactor() {
    vec3  toFrag = vWorldPos - uPointLightPos;
    float cur    = length(toFrag) / uPointFar;      // normalized [0,1]
    if (cur > 1.0) return 0.0;                       // beyond range → lit
    float bias = 0.004;                              // small: back-face depth gives the offset
    vec3 offs[6] = vec3[](vec3(1,0,0), vec3(-1,0,0), vec3(0,1,0),
                          vec3(0,-1,0), vec3(0,0,1), vec3(0,0,-1));
    float sh = (cur - bias > texture(uPointShadow, toFrag).r) ? 1.0 : 0.0;
    for (int i = 0; i < 6; ++i)
        sh += (cur - bias > texture(uPointShadow, toFrag + offs[i] * 0.012).r) ? 1.0 : 0.0;
    return sh / 7.0;
}

// ── Image-based lighting (ambient) ──────────────────────────────────────────
uniform samplerCube uIrradiance;
uniform samplerCube uPrefilter;
uniform sampler2D   uBrdfLUT;
uniform int         uHasIBL;

// Screen-space ambient occlusion (sampled by screen position).
uniform sampler2D   uSSAO;
uniform int         uSSAOEnabled;
uniform vec2        uScreenSize;

float sceneAO(float baseAO) {
    if (uSSAOEnabled == 1)
        return baseAO * texture(uSSAO, gl_FragCoord.xy / uScreenSize).r;
    return baseAO;
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 iblAmbient(vec3 N, vec3 V, vec3 albedo, float metallic, float roughness, float aoIn) {
    float ao = sceneAO(aoIn);                               // fold in SSAO
    if (uHasIBL == 0) return vec3(0.04) * albedo * ao;     // flat fallback

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F  = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    vec3 kD = (1.0 - F) * (1.0 - metallic);

    vec3 irradiance = texture(uIrradiance, N).rgb;
    vec3 diffuse    = irradiance * albedo;

    vec3  R = reflect(-V, N);
    const float MAX_LOD = 4.0;
    vec3 prefiltered = textureLod(uPrefilter, R, roughness * MAX_LOD).rgb;
    vec2 brdf        = texture(uBrdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
    vec3 specular    = prefiltered * (F * brdf.x + brdf.y);

    return (kD * diffuse + specular) * ao;
}

vec3 brdfTerm(vec3 N, vec3 V, vec3 L, vec3 albedo, float metallic, float roughness) {
    vec3  H     = normalize(V + L);
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    vec3  F0    = mix(vec3(0.04), albedo, metallic);
    vec3  F     = F_Schlick(max(dot(H, V), 0.0), F0);
    float D     = D_GGX(NdotH, roughness);
    float G     = G_Smith(NdotV, NdotL, roughness);
    vec3  spec  = (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);
    vec3  kD    = (vec3(1.0) - F) * (1.0 - metallic);
    return (kD * albedo / PI + spec) * NdotL;
}

vec3 shadePBR(vec3 N, vec3 V, vec3 albedo, float metallic, float roughness) {
    // No scene lights → fall back to a fixed key light so nothing is pitch black.
    if (uNumLights <= 0) {
        vec3 L = normalize(vec3(1.2, 2.0, 0.8));
        return brdfTerm(N, V, L, albedo, metallic, roughness) * vec3(3.2, 3.0, 2.8);
    }

    vec3 Lo = vec3(0.0);
    for (int i = 0; i < MAX_LIGHTS; ++i) {
        if (i >= uNumLights) break;

        vec3 L;
        vec3 radiance = uLightColor[i];

        if (uLightType[i] == 0) {            // directional
            L = normalize(-uLightDir[i]);
        } else {                             // point / spot
            vec3  toL  = uLightPos[i] - vWorldPos;
            float dist = length(toL);
            L = toL / max(dist, 1e-4);

            float r   = max(uLightRange[i], 1e-4);
            float win = clamp(1.0 - pow(dist / r, 4.0), 0.0, 1.0);
            float att = (win * win) / (dist * dist + 1.0);
            radiance *= att;

            if (uLightType[i] == 2) {        // spot cone
                float c = dot(normalize(uLightDir[i]), -L);
                float t = clamp((c - uLightOuterCos[i]) /
                                max(uLightInnerCos[i] - uLightOuterCos[i], 1e-4), 0.0, 1.0);
                radiance *= t;
            }
        }

        float sh = 0.0;
        if      (i == uShadowLight)      sh = shadowFactor(N, L);
        else if (i == uPointShadowLight) sh = pointShadowFactor();
        Lo += brdfTerm(N, V, L, albedo, metallic, roughness) * radiance * (1.0 - sh);
    }
    return Lo;
}
)GLSL";
