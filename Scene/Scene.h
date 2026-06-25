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
