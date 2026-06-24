#include "MaterialGraph.h"
#include <fstream>
#include <sstream>
#include <cstring>

static GraphPin MakePin(int& id, bool isInput, PinType type, const char* name) {
    GraphPin p;
    p.id      = id++;
    p.isInput = isInput;
    p.type    = type;
    p.name    = name;
    return p;
}

GraphNode MakeNode(NodeKind kind, int& nextId) {
    GraphNode n;
    n.id   = nextId++;
    n.kind = kind;

    auto inp = [&](PinType t, const char* nm){ return MakePin(nextId, true,  t, nm); };
    auto out = [&](PinType t, const char* nm){ return MakePin(nextId, false, t, nm); };

    switch (kind) {
        case NodeKind::MaterialOutput:
            n.inputs = { inp(PinType::Vec3,"Base Color"), inp(PinType::Float,"Metallic"),
                         inp(PinType::Float,"Roughness"), inp(PinType::Vec3,"Emissive"),
                         inp(PinType::Vec3,"Normal") };
            n.inputs[0].defVec3[0]=0.8f; n.inputs[0].defVec3[1]=0.8f; n.inputs[0].defVec3[2]=0.8f;
            n.inputs[1].defFloat = 0.0f;
            n.inputs[2].defFloat = 0.5f;
            n.inputs[3].defVec3[0]=0.0f; n.inputs[3].defVec3[1]=0.0f; n.inputs[3].defVec3[2]=0.0f;
            n.inputs[4].defVec3[0]=0.0f; n.inputs[4].defVec3[1]=0.0f; n.inputs[4].defVec3[2]=1.0f;
            break;
        case NodeKind::NormalMap:
            n.inputs  = { inp(PinType::Vec2,"UV") };
            n.outputs = { out(PinType::Vec3,"Normal") };
            break;
        case NodeKind::Time:
            n.outputs = { out(PinType::Float,"Time") };
            break;
        case NodeKind::Fresnel:
            n.inputs  = { inp(PinType::Float,"Power") };
            n.inputs[0].defFloat = 5.0f;
            n.outputs = { out(PinType::Float,"Fresnel") };
            break;
        case NodeKind::Panner:
            n.inputs  = { inp(PinType::Vec2,"UV"), inp(PinType::Float,"Speed X"),
                          inp(PinType::Float,"Speed Y") };
            n.inputs[1].defFloat = 0.1f;
            n.outputs = { out(PinType::Vec2,"UV") };
            break;
        case NodeKind::Rotator:
            n.inputs  = { inp(PinType::Vec2,"UV"), inp(PinType::Float,"Angle") };
            n.outputs = { out(PinType::Vec2,"UV") };
            break;
        case NodeKind::Remap:
            n.inputs  = { inp(PinType::Float,"Value"), inp(PinType::Float,"In Min"),
                          inp(PinType::Float,"In Max"), inp(PinType::Float,"Out Min"),
                          inp(PinType::Float,"Out Max") };
            n.inputs[2].defFloat = 1.0f;
            n.inputs[4].defFloat = 1.0f;
            n.outputs = { out(PinType::Float,"Result") };
            break;
        case NodeKind::NormalStrength:
            n.inputs  = { inp(PinType::Vec3,"Normal"), inp(PinType::Float,"Strength") };
            n.inputs[1].defFloat = 1.0f;
            n.outputs = { out(PinType::Vec3,"Normal") };
            break;
        case NodeKind::Color:
            n.outputs = { out(PinType::Vec3,"Color") };
            break;
        case NodeKind::Float:
            n.outputs = { out(PinType::Float,"Value") };
            break;
        case NodeKind::Texture2D:
            n.inputs  = { inp(PinType::Vec2,"UV") };
            n.outputs = { out(PinType::Vec4,"RGBA"), out(PinType::Vec3,"RGB"),
                          out(PinType::Float,"A") };
            break;
        case NodeKind::UV:
            n.outputs = { out(PinType::Vec2,"UV") };
            break;
        case NodeKind::Multiply:
            n.inputs  = { inp(PinType::Vec3,"A"), inp(PinType::Vec3,"B") };
            n.outputs = { out(PinType::Vec3,"Result") };
            break;
        case NodeKind::Add:
            n.inputs  = { inp(PinType::Vec3,"A"), inp(PinType::Vec3,"B") };
            n.outputs = { out(PinType::Vec3,"Result") };
            break;
        case NodeKind::Mix:
            n.inputs  = { inp(PinType::Vec3,"A"), inp(PinType::Vec3,"B"),
                          inp(PinType::Float,"Alpha") };
            n.outputs = { out(PinType::Vec3,"Result") };
            break;
        case NodeKind::OneMinus:
            n.inputs  = { inp(PinType::Vec3,"Value") };
            n.outputs = { out(PinType::Vec3,"Result") };
            break;
        case NodeKind::Power:
            n.inputs  = { inp(PinType::Vec3,"Base"), inp(PinType::Float,"Exp") };
            n.outputs = { out(PinType::Vec3,"Result") };
            break;
        case NodeKind::Clamp:
            n.inputs  = { inp(PinType::Vec3,"Value") };
            n.outputs = { out(PinType::Vec3,"Result") };
            break;
        case NodeKind::SplitRGB:
            n.inputs  = { inp(PinType::Vec3,"RGB") };
            n.outputs = { out(PinType::Float,"R"), out(PinType::Float,"G"),
                          out(PinType::Float,"B") };
            break;
        case NodeKind::MakeRGB:
            n.inputs  = { inp(PinType::Float,"R"), inp(PinType::Float,"G"),
                          inp(PinType::Float,"B") };
            n.outputs = { out(PinType::Vec3,"RGB") };
            break;
    }
    return n;
}

const char* GraphNode::Title() const {
    switch (kind) {
        case NodeKind::Float:          return "Float";
        case NodeKind::Color:          return "Color";
        case NodeKind::Texture2D:      return "Texture2D";
        case NodeKind::UV:             return "UV";
        case NodeKind::Multiply:       return "Multiply";
        case NodeKind::Add:            return "Add";
        case NodeKind::Mix:            return "Mix";
        case NodeKind::OneMinus:       return "One Minus";
        case NodeKind::Power:          return "Power";
        case NodeKind::Clamp:          return "Clamp";
        case NodeKind::SplitRGB:       return "Split RGB";
        case NodeKind::MakeRGB:        return "Make RGB";
        case NodeKind::NormalMap:      return "Normal Map";
        case NodeKind::Time:           return "Time";
        case NodeKind::Fresnel:        return "Fresnel";
        case NodeKind::Panner:         return "Panner";
        case NodeKind::Rotator:        return "Rotator";
        case NodeKind::Remap:          return "Remap";
        case NodeKind::NormalStrength: return "Normal Strength";
        case NodeKind::MaterialOutput: return "Material Output";
    }
    return "Unknown";
}

GraphNode* MaterialGraph::FindNode(int nodeId) {
    for (auto& n : nodes) if (n.id == nodeId) return &n;
    return nullptr;
}

GraphNode* MaterialGraph::FindNodeByOutputPin(int pinId) {
    for (auto& n : nodes)
        for (auto& p : n.outputs)
            if (p.id == pinId) return &n;
    return nullptr;
}

GraphNode* MaterialGraph::FindNodeByInputPin(int pinId) {
    for (auto& n : nodes)
        for (auto& p : n.inputs)
            if (p.id == pinId) return &n;
    return nullptr;
}

GraphPin* MaterialGraph::FindPin(int pinId, GraphNode** outNode) {
    for (auto& n : nodes) {
        for (auto& p : n.inputs)  if (p.id == pinId) { if (outNode) *outNode = &n; return &p; }
        for (auto& p : n.outputs) if (p.id == pinId) { if (outNode) *outNode = &n; return &p; }
    }
    return nullptr;
}

GraphLink* MaterialGraph::FindLinkToPin(int toPinId) {
    for (auto& l : links) if (l.toPin == toPinId) return &l;
    return nullptr;
}

PinType MaterialGraph::OutputPinType(int pinId) const {
    for (auto& n : nodes)
        for (auto& p : n.outputs)
            if (p.id == pinId) return p.type;
    return PinType::Vec3;
}

void MaterialGraph::AddDefaultOutput() {
    GraphNode out = MakeNode(NodeKind::MaterialOutput, nextId);
    out.pos = {500.0f, 200.0f};
    nodes.push_back(std::move(out));
}

void MaterialGraph::AddNode(NodeKind kind, ImVec2 editorPos) {
    GraphNode n = MakeNode(kind, nextId);
    n.pos = editorPos;
    nodes.push_back(std::move(n));
}

static const char* KindToStr(NodeKind k) {
    switch (k) {
        case NodeKind::Float:          return "Float";
        case NodeKind::Color:          return "Color";
        case NodeKind::Texture2D:      return "Texture2D";
        case NodeKind::UV:             return "UV";
        case NodeKind::Multiply:       return "Multiply";
        case NodeKind::Add:            return "Add";
        case NodeKind::Mix:            return "Mix";
        case NodeKind::OneMinus:       return "OneMinus";
        case NodeKind::Power:          return "Power";
        case NodeKind::Clamp:          return "Clamp";
        case NodeKind::SplitRGB:       return "SplitRGB";
        case NodeKind::MakeRGB:        return "MakeRGB";
        case NodeKind::NormalMap:      return "NormalMap";
        case NodeKind::Time:           return "Time";
        case NodeKind::Fresnel:        return "Fresnel";
        case NodeKind::Panner:         return "Panner";
        case NodeKind::Rotator:        return "Rotator";
        case NodeKind::Remap:          return "Remap";
        case NodeKind::NormalStrength: return "NormalStrength";
        case NodeKind::MaterialOutput: return "MaterialOutput";
    }
    return "Float";
}

static NodeKind StrToKind(const std::string& s) {
    if (s == "Color")          return NodeKind::Color;
    if (s == "Texture2D")      return NodeKind::Texture2D;
    if (s == "UV")             return NodeKind::UV;
    if (s == "Multiply")       return NodeKind::Multiply;
    if (s == "Add")            return NodeKind::Add;
    if (s == "Mix")            return NodeKind::Mix;
    if (s == "OneMinus")       return NodeKind::OneMinus;
    if (s == "Power")          return NodeKind::Power;
    if (s == "Clamp")          return NodeKind::Clamp;
    if (s == "SplitRGB")       return NodeKind::SplitRGB;
    if (s == "MakeRGB")        return NodeKind::MakeRGB;
    if (s == "NormalMap")      return NodeKind::NormalMap;
    if (s == "Time")           return NodeKind::Time;
    if (s == "Fresnel")        return NodeKind::Fresnel;
    if (s == "Panner")         return NodeKind::Panner;
    if (s == "Rotator")        return NodeKind::Rotator;
    if (s == "Remap")          return NodeKind::Remap;
    if (s == "NormalStrength") return NodeKind::NormalStrength;
    if (s == "MaterialOutput") return NodeKind::MaterialOutput;
    return NodeKind::Float;
}

bool MaterialGraph::Save(const std::string& graphPath) const {
    std::ofstream f(graphPath);
    if (!f.is_open()) return false;
    f << "nextId " << nextId << "\n";
    for (auto& n : nodes) {
        f << "node " << n.id << " " << KindToStr(n.kind)
          << " " << n.pos.x << " " << n.pos.y;
        switch (n.kind) {
            case NodeKind::Color:
                f << " " << n.color[0] << " " << n.color[1] << " " << n.color[2]; break;
            case NodeKind::Float:
                f << " " << n.floatVal; break;
            case NodeKind::Texture2D:
            case NodeKind::NormalMap:
                f << " " << (n.texPath.empty() ? "none" : n.texPath); break;
            default: break;
        }
        f << "\n";
        // Persist per-input default values (used for unconnected inputs).
        for (size_t i = 0; i < n.inputs.size(); ++i) {
            const GraphPin& p = n.inputs[i];
            f << "pin " << i << " " << p.defFloat << " "
              << p.defVec3[0] << " " << p.defVec3[1] << " " << p.defVec3[2] << "\n";
        }
    }
    for (auto& l : links)
        f << "link " << l.id << " " << l.fromPin << " " << l.toPin << "\n";
    return true;
}

bool MaterialGraph::Load(const std::string& graphPath) {
    std::ifstream f(graphPath);
    if (!f.is_open()) return false;
    nodes.clear();
    links.clear();
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::string tag; ss >> tag;
        if (tag == "nextId") {
            ss >> nextId;
        } else if (tag == "node") {
            int id; std::string kindStr; float x, y;
            ss >> id >> kindStr >> x >> y;
            NodeKind kind  = StrToKind(kindStr);
            int savedId    = id;
            GraphNode n    = MakeNode(kind, id);
            n.id  = savedId;
            n.pos = {x, y};
            switch (kind) {
                case NodeKind::Color: {
                    float r, g, b; ss >> r >> g >> b;
                    n.color[0]=r; n.color[1]=g; n.color[2]=b;
                } break;
                case NodeKind::Float:
                    ss >> n.floatVal; break;
                case NodeKind::Texture2D:
                case NodeKind::NormalMap: {
                    // Path may contain spaces — take the rest of the line.
                    std::string p; std::getline(ss, p);
                    size_t s = p.find_first_not_of(" \t");
                    if (s != std::string::npos) p = p.substr(s);
                    if (!p.empty() && p != "none") n.texPath = p;
                } break;
                default: break;
            }
            nodes.push_back(std::move(n));
        } else if (tag == "pin") {
            // Per-input default values for the most-recently-loaded node.
            if (!nodes.empty()) {
                size_t idx; float df, x, y, z;
                ss >> idx >> df >> x >> y >> z;
                GraphNode& n = nodes.back();
                if (idx < n.inputs.size()) {
                    n.inputs[idx].defFloat   = df;
                    n.inputs[idx].defVec3[0] = x;
                    n.inputs[idx].defVec3[1] = y;
                    n.inputs[idx].defVec3[2] = z;
                }
            }
        } else if (tag == "link") {
            GraphLink l;
            ss >> l.id >> l.fromPin >> l.toPin;
            links.push_back(l);
        }
    }
    return true;
}
