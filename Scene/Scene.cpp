#include "Scene.h"
#include <unordered_map>
#include <unordered_set>

entt::entity Scene::Create(const std::string& name) {
    entt::entity e = m_reg.create();
    m_reg.emplace<NameComponent>(e, name);
    m_reg.emplace<TransformComponent>(e);
    return e;
}

void Scene::Destroy(entt::entity e) {
    if (m_reg.valid(e)) m_reg.destroy(e);
}

PrimitiveType EntityType(const entt::registry& reg, entt::entity e) {
    if (reg.all_of<CameraComponent>(e)) return PrimitiveType::Camera;
    if (reg.all_of<LightComponent>(e))  return PrimitiveType::Light;
    if (const auto* m = reg.try_get<MeshComponent>(e)) {
        switch (m->kind) {
            case MeshKind::Cube:   return PrimitiveType::Cube;
            case MeshKind::Sphere: return PrimitiveType::Sphere;
            case MeshKind::Plane:  return PrimitiveType::Plane;
            case MeshKind::Model:  return PrimitiveType::Model;
            default: break;
        }
    }
    return PrimitiveType::Empty;
}

// ── Transform hierarchy ─────────────────────────────────────────────────────
namespace {
    // Compute (and cache) one entity's world matrix, recursing up the parent
    // chain. `done` memoizes finished entities; `active` detects cycles so a
    // malformed hierarchy degrades to local-only instead of overflowing the stack.
    const glm::mat4& ComputeWorld(entt::registry& reg, entt::entity e,
                                  std::unordered_map<entt::entity, glm::mat4>& done,
                                  std::unordered_set<entt::entity>& active) {
        if (auto it = done.find(e); it != done.end()) return it->second;

        glm::mat4 local(1.0f);
        if (const auto* t = reg.try_get<TransformComponent>(e)) local = t->Matrix();

        glm::mat4 world = local;
        const auto* p = reg.try_get<ParentComponent>(e);
        if (p && p->parent != entt::null && reg.valid(p->parent) && !active.count(p->parent)) {
            active.insert(e);
            world = ComputeWorld(reg, p->parent, done, active) * local;
            active.erase(e);
        }

        auto [it, _] = done.emplace(e, world);
        reg.emplace_or_replace<WorldTransform>(e, WorldTransform{ it->second });
        return it->second;
    }
}

void UpdateWorldTransforms(entt::registry& reg) {
    std::unordered_map<entt::entity, glm::mat4> done;
    std::unordered_set<entt::entity> active;
    for (entt::entity e : reg.view<TransformComponent>())
        ComputeWorld(reg, e, done, active);
}

glm::mat4 WorldMatrixOf(const entt::registry& reg, entt::entity e) {
    if (const auto* w = reg.try_get<WorldTransform>(e)) return w->matrix;
    if (const auto* t = reg.try_get<TransformComponent>(e)) return t->Matrix();
    return glm::mat4(1.0f);
}

bool IsAncestorOf(const entt::registry& reg, entt::entity ancestor, entt::entity e) {
    for (entt::entity cur = e; cur != entt::null && reg.valid(cur); ) {
        if (cur == ancestor) return true;
        const auto* p = reg.try_get<ParentComponent>(cur);
        cur = p ? p->parent : entt::null;
    }
    return false;
}
