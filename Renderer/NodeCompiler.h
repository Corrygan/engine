#pragma once
#include "MaterialGraph.h"
#include <string>
#include <unordered_map>

struct CompiledShader {
    bool        valid   = false;
    std::string fragSrc;
    std::string error;
    // Texture node id → uniform name mapping
    std::unordered_map<int, std::string> texUniforms;
};

class NodeCompiler {
public:
    static CompiledShader Compile(const MaterialGraph& graph);

private:
    struct Context {
        std::string                          code;
        std::string                          texUniforms;
        std::unordered_map<int,std::string>  pinVar;   // pinId → glsl expression
        std::unordered_map<int,std::string>  pinType;  // pinId → glsl type string
        std::unordered_map<int, std::string> texUniformMap;
        int                                  texCount = 0;
        const MaterialGraph*                 graph    = nullptr;
    };

    static void   VisitNode(int nodeId, Context& ctx, std::vector<int>& visited);
    static void   EmitNode(const GraphNode& node, Context& ctx);
    static std::string GetInput(const GraphPin& pin, PinType targetType,
                                const Context& ctx, const MaterialGraph& graph);
    static std::string Coerce(const std::string& expr, PinType from, PinType to);
    static std::string TypeStr(PinType t);
    static std::string BuildFragSrc(const std::string& texUniforms, const std::string& nodeCode,
                                    const std::string& baseColor, const std::string& metallic,
                                    const std::string& roughness, const std::string& emissive);
};
