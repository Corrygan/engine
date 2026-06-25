#include "Scene.h"

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
