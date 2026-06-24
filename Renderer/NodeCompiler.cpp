#include "NodeCompiler.h"
#include "LightingGLSL.h"
#include <sstream>
#include <functional>
#include <algorithm>
#include <cstdio>

// ─── Helpers ───────────────────────────────────────────────────────────────

std::string NodeCompiler::TypeStr(PinType t) {
    switch (t) {
        case PinType::Float: return "float";
        case PinType::Vec2:  return "vec2";
        case PinType::Vec3:  return "vec3";
        case PinType::Vec4:  return "vec4";
    }
    return "float";
}

std::string NodeCompiler::Coerce(const std::string& expr, PinType from, PinType to) {
    if (from == to) return expr;
    if (from == PinType::Float && to == PinType::Vec3) return "vec3(" + expr + ")";
    if (from == PinType::Float && to == PinType::Vec4) return "vec4(vec3(" + expr + "),1.0)";
    if (from == PinType::Vec3  && to == PinType::Float) return "(" + expr + ").x";
    if (from == PinType::Vec3  && to == PinType::Vec4) return "vec4(" + expr + ",1.0)";
    if (from == PinType::Vec4  && to == PinType::Vec3) return "(" + expr + ").rgb";
    if (from == PinType::Vec4  && to == PinType::Float) return "(" + expr + ").r";
    if (from == PinType::Vec2  && to == PinType::Vec3) return "vec3(" + expr + ",0.0)";
    return expr;
}

std::string NodeCompiler::GetInput(const GraphPin& pin, PinType targetType,
                                   const Context& ctx, const MaterialGraph& graph) {
    auto* link = const_cast<MaterialGraph&>(graph).FindLinkToPin(pin.id);
    if (link) {
        auto it = ctx.pinVar.find(link->fromPin);
        if (it != ctx.pinVar.end()) {
            PinType srcType = const_cast<MaterialGraph&>(graph).OutputPinType(link->fromPin);
            return Coerce(it->second, srcType, targetType);
        }
    }
    // Default value
    char buf[128];
    if (targetType == PinType::Float)
        snprintf(buf, sizeof(buf), "%f", pin.defFloat);
    else if (targetType == PinType::Vec3)
        snprintf(buf, sizeof(buf), "vec3(%f,%f,%f)", pin.defVec3[0], pin.defVec3[1], pin.defVec3[2]);
    else if (targetType == PinType::Vec2)
        snprintf(buf, sizeof(buf), "vec2(%f,%f)", pin.defFloat, pin.defFloat);
    else
        snprintf(buf, sizeof(buf), "vec4(%f,%f,%f,1.0)", pin.defVec3[0], pin.defVec3[1], pin.defVec3[2]);
    return buf;
}

// ─── Emit per-node GLSL ────────────────────────────────────────────────────

void NodeCompiler::EmitNode(const GraphNode& node, Context& ctx) {
    const MaterialGraph& g = *ctx.graph;
    char var[64];
    snprintf(var, sizeof(var), "nd%d", node.id);

    auto inp = [&](int idx, PinType t) -> std::string {
        return GetInput(node.inputs[idx], t, ctx, g);
    };

    switch (node.kind) {
        case NodeKind::Float: {
            char buf[64]; snprintf(buf, sizeof(buf), "%f", node.floatVal);
            ctx.code += "    float " + std::string(var) + " = " + buf + ";\n";
            ctx.pinVar[node.outputs[0].id]  = var;
            ctx.pinType[node.outputs[0].id] = "float";
            break;
        }
        case NodeKind::Color: {
            char buf[128];
            snprintf(buf, sizeof(buf), "vec3(%f,%f,%f)", node.color[0], node.color[1], node.color[2]);
            ctx.code += "    vec3 " + std::string(var) + " = " + buf + ";\n";
            ctx.pinVar[node.outputs[0].id]  = var;
            ctx.pinType[node.outputs[0].id] = "vec3";
            break;
        }
        case NodeKind::UV: {
            ctx.code += "    vec2 " + std::string(var) + " = vUV;\n";
            ctx.pinVar[node.outputs[0].id]  = var;
            ctx.pinType[node.outputs[0].id] = "vec2";
            break;
        }
        case NodeKind::Texture2D: {
            char uname[64]; snprintf(uname, sizeof(uname), "uTex%d", node.id);
            ctx.texUniforms += "uniform sampler2D " + std::string(uname) + ";\n";
            ctx.texUniformMap[node.id] = uname;

            std::string uvExpr = "vUV";
            auto* uvLink = const_cast<MaterialGraph&>(g).FindLinkToPin(node.inputs[0].id);
            if (uvLink) {
                auto it = ctx.pinVar.find(uvLink->fromPin);
                if (it != ctx.pinVar.end()) uvExpr = it->second;
            }

            std::string raw = std::string(var) + "_raw";
            ctx.code += "    vec4 " + raw + " = texture(" + std::string(uname) + ", " + uvExpr + ");\n";
            ctx.code += "    vec4 " + std::string(var) + "_rgba = " + raw + ";\n";
            ctx.code += "    vec3 " + std::string(var) + "_rgb = " + raw + ".rgb;\n";
            ctx.code += "    float " + std::string(var) + "_a = " + raw + ".a;\n";

            ctx.pinVar[node.outputs[0].id]  = std::string(var) + "_rgba";
            ctx.pinVar[node.outputs[1].id]  = std::string(var) + "_rgb";
            ctx.pinVar[node.outputs[2].id]  = std::string(var) + "_a";
            break;
        }
        case NodeKind::Multiply: {
            std::string a = inp(0, PinType::Vec3), b = inp(1, PinType::Vec3);
            ctx.code += "    vec3 " + std::string(var) + " = " + a + " * " + b + ";\n";
            ctx.pinVar[node.outputs[0].id] = var;
            break;
        }
        case NodeKind::Add: {
            std::string a = inp(0, PinType::Vec3), b = inp(1, PinType::Vec3);
            ctx.code += "    vec3 " + std::string(var) + " = " + a + " + " + b + ";\n";
            ctx.pinVar[node.outputs[0].id] = var;
            break;
        }
        case NodeKind::Mix: {
            std::string a = inp(0, PinType::Vec3), b = inp(1, PinType::Vec3), t = inp(2, PinType::Float);
            ctx.code += "    vec3 " + std::string(var) + " = mix(" + a + "," + b + "," + t + ");\n";
            ctx.pinVar[node.outputs[0].id] = var;
            break;
        }
        case NodeKind::OneMinus: {
            std::string v = inp(0, PinType::Vec3);
            ctx.code += "    vec3 " + std::string(var) + " = vec3(1.0) - " + v + ";\n";
            ctx.pinVar[node.outputs[0].id] = var;
            break;
        }
        case NodeKind::Power: {
            std::string base = inp(0, PinType::Vec3), exp = inp(1, PinType::Float);
            ctx.code += "    vec3 " + std::string(var) + " = pow(max(" + base + ",vec3(0.0)),vec3(" + exp + "));\n";
            ctx.pinVar[node.outputs[0].id] = var;
            break;
        }
        case NodeKind::Clamp: {
            std::string v = inp(0, PinType::Vec3);
            ctx.code += "    vec3 " + std::string(var) + " = clamp(" + v + ",0.0,1.0);\n";
            ctx.pinVar[node.outputs[0].id] = var;
            break;
        }
        case NodeKind::SplitRGB: {
            std::string v = inp(0, PinType::Vec3);
            ctx.code += "    float " + std::string(var) + "_r = (" + v + ").r;\n";
            ctx.code += "    float " + std::string(var) + "_g = (" + v + ").g;\n";
            ctx.code += "    float " + std::string(var) + "_b = (" + v + ").b;\n";
            ctx.pinVar[node.outputs[0].id] = std::string(var) + "_r";
            ctx.pinVar[node.outputs[1].id] = std::string(var) + "_g";
            ctx.pinVar[node.outputs[2].id] = std::string(var) + "_b";
            break;
        }
        case NodeKind::MakeRGB: {
            std::string r = inp(0, PinType::Float), gr = inp(1, PinType::Float), b = inp(2, PinType::Float);
            ctx.code += "    vec3 " + std::string(var) + " = vec3(" + r + "," + gr + "," + b + ");\n";
            ctx.pinVar[node.outputs[0].id] = var;
            break;
        }
        case NodeKind::NormalMap: {
            char uname[64]; snprintf(uname, sizeof(uname), "uTex%d", node.id);
            ctx.texUniforms += "uniform sampler2D " + std::string(uname) + ";\n";
            ctx.texUniformMap[node.id] = uname;

            std::string uvExpr = "vUV";
            auto* uvLink = const_cast<MaterialGraph&>(g).FindLinkToPin(node.inputs[0].id);
            if (uvLink) {
                auto it = ctx.pinVar.find(uvLink->fromPin);
                if (it != ctx.pinVar.end()) uvExpr = it->second;
            }
            // Unpack to tangent-space normal.
            ctx.code += "    vec3 " + std::string(var) + " = texture(" + std::string(uname) +
                        ", " + uvExpr + ").rgb * 2.0 - 1.0;\n";
            ctx.pinVar[node.outputs[0].id] = var;
            break;
        }
        case NodeKind::Time: {
            ctx.code += "    float " + std::string(var) + " = uTime;\n";
            ctx.pinVar[node.outputs[0].id] = var;
            break;
        }
        case NodeKind::Fresnel: {
            std::string power = inp(0, PinType::Float);
            ctx.code += "    float " + std::string(var) +
                        " = pow(clamp(1.0 - max(dot(Ng, V), 0.0), 0.0, 1.0), max(" +
                        power + ", 0.0001));\n";
            ctx.pinVar[node.outputs[0].id] = var;
            break;
        }
        case NodeKind::Panner: {
            // UV input defaults to vUV when unconnected.
            std::string uv = "vUV";
            auto* uvLink = const_cast<MaterialGraph&>(g).FindLinkToPin(node.inputs[0].id);
            if (uvLink) { auto it = ctx.pinVar.find(uvLink->fromPin);
                          if (it != ctx.pinVar.end()) uv = it->second; }
            std::string sx = inp(1, PinType::Float), sy = inp(2, PinType::Float);
            ctx.code += "    vec2 " + std::string(var) + " = (" + uv +
                        ") + uTime * vec2(" + sx + ", " + sy + ");\n";
            ctx.pinVar[node.outputs[0].id] = var;
            break;
        }
        case NodeKind::Rotator: {
            std::string uv = "vUV";
            auto* uvLink = const_cast<MaterialGraph&>(g).FindLinkToPin(node.inputs[0].id);
            if (uvLink) { auto it = ctx.pinVar.find(uvLink->fromPin);
                          if (it != ctx.pinVar.end()) uv = it->second; }
            std::string ang = inp(1, PinType::Float);
            std::string v = var;
            ctx.code += "    vec2 " + v + "_c = (" + uv + ") - 0.5;\n";
            ctx.code += "    float " + v + "_a = " + ang + ";\n";
            ctx.code += "    vec2 " + v + " = vec2(" + v + "_c.x*cos(" + v + "_a) - " + v +
                        "_c.y*sin(" + v + "_a), " + v + "_c.x*sin(" + v + "_a) + " + v +
                        "_c.y*cos(" + v + "_a)) + 0.5;\n";
            ctx.pinVar[node.outputs[0].id] = var;
            break;
        }
        case NodeKind::Remap: {
            std::string v  = inp(0, PinType::Float);
            std::string i0 = inp(1, PinType::Float), i1 = inp(2, PinType::Float);
            std::string o0 = inp(3, PinType::Float), o1 = inp(4, PinType::Float);
            ctx.code += "    float " + std::string(var) + " = " + o0 + " + (" + v + " - " + i0 +
                        ") * (" + o1 + " - " + o0 + ") / max((" + i1 + " - " + i0 + "), 1e-5);\n";
            ctx.pinVar[node.outputs[0].id] = var;
            break;
        }
        case NodeKind::NormalStrength: {
            std::string n = inp(0, PinType::Vec3), s = inp(1, PinType::Float);
            std::string v = var;
            ctx.code += "    vec3 " + v + "_n = " + n + ";\n";
            ctx.code += "    vec3 " + v + " = normalize(vec3(" + v + "_n.xy * " + s + ", max(" +
                        v + "_n.z, 0.01)));\n";
            ctx.pinVar[node.outputs[0].id] = var;
            break;
        }
        case NodeKind::MaterialOutput:
            break;
    }
}

void NodeCompiler::VisitNode(int nodeId, Context& ctx, std::vector<int>& visited) {
    if (std::find(visited.begin(), visited.end(), nodeId) != visited.end()) return;
    visited.push_back(nodeId);

    const MaterialGraph& g = *ctx.graph;
    const GraphNode* node = const_cast<MaterialGraph&>(g).FindNode(nodeId);
    if (!node) return;

    for (auto& pin : node->inputs) {
        auto* link = const_cast<MaterialGraph&>(g).FindLinkToPin(pin.id);
        if (link) {
            const GraphNode* src = const_cast<MaterialGraph&>(g).FindNodeByOutputPin(link->fromPin);
            if (src) VisitNode(src->id, ctx, visited);
        }
    }
    EmitNode(*node, ctx);
}

// ─── Main compile ──────────────────────────────────────────────────────────

CompiledShader NodeCompiler::Compile(const MaterialGraph& graph) {
    CompiledShader result;

    const GraphNode* outputNode = nullptr;
    for (auto& n : graph.nodes)
        if (n.kind == NodeKind::MaterialOutput) { outputNode = &n; break; }

    if (!outputNode) {
        result.error = "No MaterialOutput node";
        return result;
    }

    Context ctx;
    ctx.graph = &graph;

    std::vector<int> visited;
    VisitNode(outputNode->id, ctx, visited);

    auto getOut = [&](int inputIndex, PinType t, const std::string& def) -> std::string {
        return GetInput(outputNode->inputs[inputIndex], t, ctx, graph);
    };

    std::string baseColor = getOut(0, PinType::Vec3, "vec3(0.8)");
    std::string metallic  = getOut(1, PinType::Float, "0.0");
    std::string roughness = getOut(2, PinType::Float, "0.5");
    std::string emissive  = getOut(3, PinType::Vec3, "vec3(0.0)");

    // Normal input (index 4) is tangent-space; only used when something is wired in.
    std::string normalExpr;
    bool        hasNormal = false;
    if (outputNode->inputs.size() > 4) {
        auto* nl = const_cast<MaterialGraph&>(graph).FindLinkToPin(outputNode->inputs[4].id);
        if (nl) {
            hasNormal  = true;
            normalExpr = GetInput(outputNode->inputs[4], PinType::Vec3, ctx, graph);
        }
    }

    result.fragSrc     = BuildFragSrc(ctx.texUniforms, ctx.code, baseColor, metallic, roughness,
                                      emissive, normalExpr, hasNormal);
    result.texUniforms = ctx.texUniformMap;
    result.valid       = true;
    return result;
}

std::string NodeCompiler::BuildFragSrc(const std::string& texUniforms, const std::string& nodeCode,
                                       const std::string& baseColor, const std::string& metallic,
                                       const std::string& roughness, const std::string& emissive,
                                       const std::string& normalExpr, bool hasNormal) {
    std::string src = R"(
#version 330 core
in vec3 vNormal;
in vec3 vWorldPos;
in vec2 vUV;
in vec3 vTangent;
in vec3 vBitangent;

uniform vec3 uCamPos;
uniform vec3 uSelectedColor;
uniform float uTime;
uniform int  uTonemap;   // 1 = tonemap+gamma here (preview); 0 = linear HDR (scene post)
)";
    src += texUniforms;
    src += R"(
out vec4 FragColor;
const float PI = 3.14159265359;
float D_GGX(float NdotH,float r){float a=r*r;float a2=a*a;float d=NdotH*NdotH*(a2-1.0)+1.0;return a2/(PI*d*d);}
float G_Smith(float NdotV,float NdotL,float r){float k=(r+1.0)*(r+1.0)/8.0;return(NdotV/(NdotV*(1.0-k)+k))*(NdotL/(NdotL*(1.0-k)+k));}
vec3 F_Schlick(float c,vec3 F0){return F0+(1.0-F0)*pow(clamp(1.0-c,0.0,1.0),5.0);}
)";
    src += kLightingGLSL;        // shared scene-light loop (shadePBR)
    src += R"(
// Tangent frame from screen-space derivatives — works without vertex tangents.
mat3 cotangentFrame(vec3 N, vec3 p, vec2 uv){
    vec3 dp1 = dFdx(p);  vec3 dp2 = dFdy(p);
    vec2 du1 = dFdx(uv); vec2 du2 = dFdy(uv);
    vec3 dp2perp = cross(dp2, N);
    vec3 dp1perp = cross(N, dp1);
    vec3 T = dp2perp*du1.x + dp1perp*du2.x;
    vec3 B = dp2perp*du1.y + dp1perp*du2.y;
    float invmax = inversesqrt(max(dot(T,T), dot(B,B)));
    return mat3(T*invmax, B*invmax, N);
}
void main(){
    vec3 Ng = normalize(vNormal);                 // geometric normal (for Fresnel etc.)
    vec3 V  = normalize(uCamPos - vWorldPos);
)";
    src += nodeCode;
    src += "    vec3 albedoLin = pow(" + baseColor + ", vec3(2.2));\n";
    src += "    float met  = clamp(" + metallic  + ", 0.0, 1.0);\n";
    src += "    float rou  = max("   + roughness + ", 0.04);\n";
    src += "    vec3 emi   = "       + emissive  + ";\n";
    if (hasNormal) {
        src += "    vec3 N  = normalize(cotangentFrame(Ng, vWorldPos, vUV) * normalize("
               + normalExpr + "));\n";
    } else {
        src += "    vec3 N = Ng;\n";
    }
    src += R"(
    vec3 Lo = shadePBR(N, V, albedoLin, met, rou);
    vec3 ambient = iblAmbient(N, V, albedoLin, met, rou, 1.0);
    vec3 color=ambient+Lo+emi;
    if (uTonemap == 1) {                  // preview path
        color = color/(color+vec3(1.0));
        color = pow(color, vec3(1.0/2.2));
    }
    FragColor=vec4(color,1.0);
}
)";
    return src;
}
