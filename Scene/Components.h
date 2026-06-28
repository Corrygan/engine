#pragma once
#include <cstdint>
#include <string>
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

// ── Data-oriented components ────────────────────────────────────────────────
// An entity is just an id; behavior/data live in these plain structs stored in
// EnTT pools. Composition replaces the old monolithic GameObject:
//   light  = entity with LightComponent (no MeshComponent)
//   camera = entity with CameraComponent
//   empty  = entity with neither

enum class MeshKind { None, Cube, Sphere, Plane, Model };

// Authoring archetype tag. Behavior is driven by which components an entity has;
// this enum survives only for the inspector's type label, the "create X" menu,
// and as the scene file's on-disk discriminator.
enum class PrimitiveType { Cube, Sphere, Camera, Light, Empty, Plane, Model };

inline const char* ToString(PrimitiveType type) {
    switch (type) {
        case PrimitiveType::Cube:   return "Cube";
        case PrimitiveType::Sphere: return "Sphere";
        case PrimitiveType::Camera: return "Camera";
        case PrimitiveType::Light:  return "Light";
        case PrimitiveType::Empty:  return "Empty";
        case PrimitiveType::Plane:  return "Plane";
        case PrimitiveType::Model:  return "Model";
    }
    return "Unknown";
}

inline MeshKind MeshKindFromPrimitive(PrimitiveType t) {
    switch (t) {
        case PrimitiveType::Cube:   return MeshKind::Cube;
        case PrimitiveType::Sphere: return MeshKind::Sphere;
        case PrimitiveType::Plane:  return MeshKind::Plane;
        case PrimitiveType::Model:  return MeshKind::Model;
        default:                    return MeshKind::None;   // Camera / Light / Empty
    }
}

struct NameComponent {
    std::string name;
};

struct TransformComponent {
    glm::vec3 position{ 0.0f };
    glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };   // w, x, y, z
    glm::vec3 scale{ 1.0f };

    // LOCAL transform (relative to the parent, if any). World = parent.world * this.
    glm::mat4 Matrix() const {
        return glm::translate(glm::mat4(1.0f), position)
             * glm::mat4_cast(rotation)
             * glm::scale(glm::mat4(1.0f), scale);
    }
};

// Parent link for the entity hierarchy. Root entities have no ParentComponent;
// a child stores its parent here and keeps a LOCAL TransformComponent. World
// transforms are derived up the chain — see WorldTransform / UpdateWorldTransforms.
struct ParentComponent {
    entt::entity parent = entt::null;
};

// Cached world-space matrix, recomputed each frame by UpdateWorldTransforms from
// the local TransformComponent and parent chain. Renderer / picking / gizmo read
// this (via WorldMatrixOf) so that moving a parent moves its children.
struct WorldTransform {
    glm::mat4 matrix{ 1.0f };
};

struct MeshComponent {
    MeshKind  kind = MeshKind::Cube;
    std::string modelPath;                          // when kind == Model
    glm::vec3 aabbMin{ -0.5f, -0.5f, -0.5f };
    glm::vec3 aabbMax{  0.5f,  0.5f,  0.5f };
};

struct MaterialComponent {
    std::string path;                               // .emat
};

struct LightComponent {
    int       type      = 0;                         // 0 dir, 1 point, 2 spot
    glm::vec3 color     { 1.0f };
    float     intensity = 3.0f;
    float     range     = 10.0f;
    float     innerDeg  = 25.0f;
    float     outerDeg  = 35.0f;
};

struct CameraComponent {
    float fov   = 60.0f;
    float nearZ = 0.1f;
    float farZ  = 1000.0f;
    bool  primary = true;
};

struct ScriptComponent {
    std::string path;                               // .lua
    bool        enabled = true;
};

// ── Built-in behaviors (one component type each) ────────────────────────────
struct RotatorComponent {
    glm::vec3 axis { 0.0f, 1.0f, 0.0f };
    float     speed = 45.0f;                         // deg/s
    bool      enabled = true;
};

struct FloaterComponent {
    glm::vec3 axis { 0.0f, 1.0f, 0.0f };
    float     speed  = 2.0f;                         // rad/s
    float     amount = 0.5f;
    bool      enabled = true;
};

// ── Physics (Jolt) ──────────────────────────────────────────────────────────
// Authoring data only. On Play, EnginePhysics builds a live Jolt body from these
// and writes the simulated transform back into TransformComponent each step; on
// Stop the scene snapshot restores the originals. An entity needs a Collider to
// take part; a RigidBody on top makes it dynamic/kinematic (else it's static).

enum class ColliderShape { Box, Sphere, Capsule };

struct ColliderComponent {
    ColliderShape shape       = ColliderShape::Box;
    glm::vec3     halfExtents { 0.5f, 0.5f, 0.5f };  // Box: half-size per axis
    float         radius      = 0.5f;                // Sphere / Capsule
    float         halfHeight  = 0.5f;                // Capsule: half the cylinder part
};

enum class BodyType { Static, Dynamic, Kinematic };

struct RigidBodyComponent {
    BodyType type        = BodyType::Dynamic;
    float    mass        = 1.0f;                     // kg (Dynamic only)
    float    friction    = 0.5f;
    float    restitution = 0.1f;                     // bounciness 0..1
    bool     startAwake  = true;
};
