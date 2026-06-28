#pragma once
#include <entt/entt.hpp>
#include <string>
#include "Components.h"

// Owns the ECS registry. Systems (rendering, scripting, physics, ...) operate
// on Scene::Reg() via EnTT views. Every entity gets Name + Transform on create.
class Scene {
public:
    entt::entity Create(const std::string& name = "Entity");
    void         Destroy(entt::entity e);
    bool         Valid(entt::entity e) const { return m_reg.valid(e); }
    void         Clear() { m_reg.clear(); }

    entt::registry&       Reg()       { return m_reg; }
    const entt::registry& Reg() const { return m_reg; }

private:
    entt::registry m_reg;
};

// Derive the legacy authoring archetype from an entity's components (inspector
// label + serializer discriminator). Priority: Camera > Light > Mesh-kind > Empty.
PrimitiveType EntityType(const entt::registry& reg, entt::entity e);

// ── Transform hierarchy ─────────────────────────────────────────────────────
// Recompute WorldTransform for every entity from its local TransformComponent up
// the parent chain. Call once per frame before rendering/picking. Assumes an
// acyclic hierarchy (the editor forbids parenting to a descendant); a cycle guard
// breaks any loop defensively.
void UpdateWorldTransforms(entt::registry& reg);

// World matrix of an entity: its cached WorldTransform if present, otherwise the
// local TransformComponent matrix (fallback when UpdateWorldTransforms hasn't run
// this frame). Returns identity if the entity has neither.
glm::mat4 WorldMatrixOf(const entt::registry& reg, entt::entity e);

// True if `ancestor` is `e` or sits above it in the parent chain. Used to reject
// reparent operations that would create a cycle.
bool IsAncestorOf(const entt::registry& reg, entt::entity ancestor, entt::entity e);
