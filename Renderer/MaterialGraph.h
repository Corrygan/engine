#pragma once
#include <string>
#include <vector>
#include <functional>
#include "../imgui/imgui.h"

enum class PinType  { Float, Vec2, Vec3, Vec4 };
enum class NodeKind {
    Float, Color, Texture2D, UV,
    Multiply, Add, Mix, OneMinus, Power, Clamp,
    SplitRGB, MakeRGB, NormalMap,
    Time, Fresnel, Panner, Rotator, Remap, NormalStrength,
    MaterialOutput
};

struct GraphPin {
    int         id       = 0;
    bool        isInput  = true;
    PinType     type     = PinType::Vec3;
    std::string name;
    float       defFloat    = 0.0f;
    float       defVec3[3]  = {1.0f, 1.0f, 1.0f};
};

struct GraphNode {
    int                   id   = 0;
    NodeKind              kind = NodeKind::Float;
    ImVec2                pos  = {0.0f, 0.0f};
    std::vector<GraphPin> inputs;
    std::vector<GraphPin> outputs;

    float       floatVal    = 0.0f;
    float       color[3]    = {1.0f, 1.0f, 1.0f};
    std::string texPath;

    const char* Title() const;
};

struct GraphLink {
    int id      = 0;
    int fromPin = 0;
    int toPin   = 0;
};

struct MaterialGraph {
    std::vector<GraphNode> nodes;
    std::vector<GraphLink> links;
    int                    nextId = 1;

    int NewId() { return nextId++; }

    GraphNode* FindNode(int nodeId);
    GraphNode* FindNodeByOutputPin(int pinId);
    GraphNode* FindNodeByInputPin(int pinId);
    GraphPin*  FindPin(int pinId, GraphNode** outNode = nullptr);
    GraphLink* FindLinkToPin(int toPinId);

    PinType    OutputPinType(int pinId) const;

    void AddDefaultOutput();
    void AddNode(NodeKind kind, ImVec2 editorPos);

    bool Save(const std::string& graphPath) const;
    bool Load(const std::string& graphPath);
};

GraphNode MakeNode(NodeKind kind, int& nextId);
