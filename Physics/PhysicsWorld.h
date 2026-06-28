#pragma once
#include <glm/glm.hpp>
#include <memory>
#include "../Scene/Scene.h"

// Result of a physics ray query.
struct RaycastHit {
    bool         hit      = false;
    entt::entity entity   = entt::null;
    glm::vec3    point{ 0.0f };
    float        distance = 0.0f;
};

// Thin wrapper over a Jolt physics world. Lifecycle:
//   Begin(scene) — build live bodies from Collider/RigidBody components
//   Step(scene)  — advance the simulation and write dynamic transforms back
//   End()        — destroy all bodies
// Jolt itself is hidden behind the pimpl so its headers/compile-defines never
// leak into the rest of the engine (only this lib links Jolt::Jolt).
class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();

    PhysicsWorld(const PhysicsWorld&)            = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    void Begin(Scene& scene);             // create bodies from components
    void Step(Scene& scene, float dt);    // simulate + write transforms back
    void End();                           // destroy bodies
    bool Active() const;

    // Ray query against the live world (Play mode). For gameplay (shooting,
    // interaction). Returns the nearest body hit, or { hit = false }.
    RaycastHit Raycast(const glm::vec3& origin, const glm::vec3& dir, float maxDistance) const;

    // Character controller (Play mode). The editor feeds a world-space move
    // direction (+ jump) each frame before Step; Step advances the capsule and
    // writes its position back into the character entity's transform.
    void SetCharacterInput(const glm::vec3& worldMoveDir, bool jump);
    bool HasCharacter() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
