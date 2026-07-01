#include "SceneSerializer.h"
#include "../Core/Guid.h"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>
#include <unordered_map>

// On-disk format is unchanged (v6) so existing .escn files keep loading. The
// flat record still uses PrimitiveType as a discriminator; we translate to/from
// the ECS components on the fly. On-disk component kinds: 0=Rotator,1=Floater,
// 2=Script (matching the old ComponentType order).
namespace {
    constexpr char     kMagic[4] = { 'E', 'S', 'C', 'N' };
    constexpr uint32_t kVersion  = 11;  // v8 physics, v9 character, v10 audio, v11 animator

    void WriteString(std::ofstream& file, const std::string& str) {
        uint32_t len = static_cast<uint32_t>(str.size());
        file.write(reinterpret_cast<const char*>(&len), sizeof(len));
        file.write(str.data(), len);
    }

    bool ReadString(std::ifstream& file, std::string& out) {
        uint32_t len = 0;
        if (!file.read(reinterpret_cast<char*>(&len), sizeof(len))) return false;
        out.resize(len);
        if (len > 0 && !file.read(out.data(), len)) return false;
        return true;
    }
}

bool SceneSerializer::Save(const std::string& path, const Scene& scene) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    file.write(kMagic, 4);
    file.write(reinterpret_cast<const char*>(&kVersion), sizeof(kVersion));

    Guid sceneGuid = Guid::Generate();
    file.write(reinterpret_cast<const char*>(sceneGuid.bytes.data()), sceneGuid.bytes.size());

    const entt::registry& reg = scene.Reg();
    auto entities = reg.view<NameComponent>();      // every entity has a name

    uint32_t count = static_cast<uint32_t>(entities.size());
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));

    // Stable index per entity (same order the loader recreates them in) so parent
    // links can be stored as a position in the file rather than a volatile id.
    std::unordered_map<entt::entity, uint32_t> indexOf;
    {
        uint32_t idx = 0;
        for (entt::entity e : entities) indexOf[e] = idx++;
    }

    for (entt::entity e : entities) {
        const PrimitiveType       type = EntityType(reg, e);
        const NameComponent&      nm   = reg.get<NameComponent>(e);
        const TransformComponent& tr   = reg.get<TransformComponent>(e);
        const MeshComponent*      mesh = reg.try_get<MeshComponent>(e);
        const MaterialComponent*  matc = reg.try_get<MaterialComponent>(e);
        const LightComponent*     lt   = reg.try_get<LightComponent>(e);

        WriteString(file, nm.name);
        uint32_t typeI = static_cast<uint32_t>(type);
        file.write(reinterpret_cast<const char*>(&typeI), sizeof(typeI));
        WriteString(file, mesh ? mesh->modelPath : std::string());
        WriteString(file, matc ? matc->path      : std::string());

        glm::vec3 amn = mesh ? mesh->aabbMin : glm::vec3(-0.5f);
        glm::vec3 amx = mesh ? mesh->aabbMax : glm::vec3( 0.5f);
        float aabbMin[3] = { amn.x, amn.y, amn.z };
        float aabbMax[3] = { amx.x, amx.y, amx.z };
        float position[3] = { tr.position.x, tr.position.y, tr.position.z };
        // on-disk quaternion order is x,y,z,w (glm stores w,x,y,z)
        float rotQuat[4]  = { tr.rotation.x, tr.rotation.y, tr.rotation.z, tr.rotation.w };
        float scale[3]    = { tr.scale.x, tr.scale.y, tr.scale.z };
        file.write(reinterpret_cast<const char*>(aabbMin),  sizeof(aabbMin));
        file.write(reinterpret_cast<const char*>(aabbMax),  sizeof(aabbMax));
        file.write(reinterpret_cast<const char*>(position), sizeof(position));
        file.write(reinterpret_cast<const char*>(rotQuat),  sizeof(rotQuat));
        file.write(reinterpret_cast<const char*>(scale),    sizeof(scale));

        // v5: light properties (defaults written when the entity isn't a light)
        int   lightType      = lt ? lt->type      : 0;
        float lightColor[3]  = { lt ? lt->color.r : 1.0f, lt ? lt->color.g : 1.0f, lt ? lt->color.b : 1.0f };
        float lightIntensity = lt ? lt->intensity : 3.0f;
        float lightRange     = lt ? lt->range     : 10.0f;
        float spotInnerDeg   = lt ? lt->innerDeg  : 25.0f;
        float spotOuterDeg   = lt ? lt->outerDeg  : 35.0f;
        file.write(reinterpret_cast<const char*>(&lightType),      sizeof(lightType));
        file.write(reinterpret_cast<const char*>(lightColor),      sizeof(lightColor));
        file.write(reinterpret_cast<const char*>(&lightIntensity), sizeof(lightIntensity));
        file.write(reinterpret_cast<const char*>(&lightRange),     sizeof(lightRange));
        file.write(reinterpret_cast<const char*>(&spotInnerDeg),   sizeof(spotInnerDeg));
        file.write(reinterpret_cast<const char*>(&spotOuterDeg),   sizeof(spotOuterDeg));

        // v6: behavior components (at most one of each in the ECS model)
        const RotatorComponent* rot = reg.try_get<RotatorComponent>(e);
        const FloaterComponent* flo = reg.try_get<FloaterComponent>(e);
        const ScriptComponent*  scr = reg.try_get<ScriptComponent>(e);

        uint32_t ccount = (rot ? 1u : 0u) + (flo ? 1u : 0u) + (scr ? 1u : 0u);
        file.write(reinterpret_cast<const char*>(&ccount), sizeof(ccount));

        auto writeComp = [&](uint32_t ctype, bool enabled, const glm::vec3& axis,
                             float speed, float amount, const std::string& scriptPath) {
            file.write(reinterpret_cast<const char*>(&ctype),   sizeof(ctype));
            file.write(reinterpret_cast<const char*>(&enabled), sizeof(enabled));
            float ax[3] = { axis.x, axis.y, axis.z };
            file.write(reinterpret_cast<const char*>(ax),       sizeof(ax));
            file.write(reinterpret_cast<const char*>(&speed),   sizeof(speed));
            file.write(reinterpret_cast<const char*>(&amount),  sizeof(amount));
            WriteString(file, scriptPath);
        };
        if (rot) writeComp(0, rot->enabled, rot->axis, rot->speed, 0.0f, std::string());
        if (flo) writeComp(1, flo->enabled, flo->axis, flo->speed, flo->amount, std::string());
        if (scr) writeComp(2, scr->enabled, glm::vec3(0, 1, 0), 0.0f, 0.0f, scr->path);

        // v7: parent link — index into this file's entity list, or -1 for a root.
        int32_t parentIndex = -1;
        if (const auto* pc = reg.try_get<ParentComponent>(e); pc && pc->parent != entt::null) {
            auto it = indexOf.find(pc->parent);
            if (it != indexOf.end()) parentIndex = static_cast<int32_t>(it->second);
        }
        file.write(reinterpret_cast<const char*>(&parentIndex), sizeof(parentIndex));

        // v8: physics — collider + rigid body (a flag byte says which are present).
        const ColliderComponent*  cc = reg.try_get<ColliderComponent>(e);
        const RigidBodyComponent* rbc = reg.try_get<RigidBodyComponent>(e);
        uint8_t physFlags = (uint8_t)((cc ? 1u : 0u) | (rbc ? 2u : 0u));
        file.write(reinterpret_cast<const char*>(&physFlags), sizeof(physFlags));
        if (cc) {
            uint32_t shape = (uint32_t)cc->shape;
            float he[3] = { cc->halfExtents.x, cc->halfExtents.y, cc->halfExtents.z };
            file.write(reinterpret_cast<const char*>(&shape), sizeof(shape));
            file.write(reinterpret_cast<const char*>(he), sizeof(he));
            file.write(reinterpret_cast<const char*>(&cc->radius),     sizeof(cc->radius));
            file.write(reinterpret_cast<const char*>(&cc->halfHeight), sizeof(cc->halfHeight));
        }
        if (rbc) {
            uint32_t bt = (uint32_t)rbc->type;
            uint8_t  awake = rbc->startAwake ? 1u : 0u;
            file.write(reinterpret_cast<const char*>(&bt),               sizeof(bt));
            file.write(reinterpret_cast<const char*>(&rbc->mass),        sizeof(rbc->mass));
            file.write(reinterpret_cast<const char*>(&rbc->friction),    sizeof(rbc->friction));
            file.write(reinterpret_cast<const char*>(&rbc->restitution), sizeof(rbc->restitution));
            file.write(reinterpret_cast<const char*>(&awake),            sizeof(awake));
        }

        // v9: character controller.
        const CharacterControllerComponent* chc = reg.try_get<CharacterControllerComponent>(e);
        uint8_t hasChar = chc ? 1u : 0u;
        file.write(reinterpret_cast<const char*>(&hasChar), sizeof(hasChar));
        if (chc) {
            uint32_t vw = (uint32_t)chc->view;
            file.write(reinterpret_cast<const char*>(&chc->height),           sizeof(chc->height));
            file.write(reinterpret_cast<const char*>(&chc->radius),           sizeof(chc->radius));
            file.write(reinterpret_cast<const char*>(&chc->moveSpeed),        sizeof(chc->moveSpeed));
            file.write(reinterpret_cast<const char*>(&chc->jumpSpeed),        sizeof(chc->jumpSpeed));
            file.write(reinterpret_cast<const char*>(&chc->eyeHeight),        sizeof(chc->eyeHeight));
            file.write(reinterpret_cast<const char*>(&chc->thirdDistance),    sizeof(chc->thirdDistance));
            file.write(reinterpret_cast<const char*>(&chc->mouseSensitivity), sizeof(chc->mouseSensitivity));
            file.write(reinterpret_cast<const char*>(&vw),                    sizeof(vw));
        }

        // v10: audio source + listener (flag byte says which are present).
        const AudioSourceComponent*   asrc = reg.try_get<AudioSourceComponent>(e);
        const AudioListenerComponent* alis = reg.try_get<AudioListenerComponent>(e);
        uint8_t audioFlags = (uint8_t)((asrc ? 1u : 0u) | (alis ? 2u : 0u));
        file.write(reinterpret_cast<const char*>(&audioFlags), sizeof(audioFlags));
        if (asrc) {
            WriteString(file, asrc->clip);
            file.write(reinterpret_cast<const char*>(&asrc->volume),      sizeof(asrc->volume));
            file.write(reinterpret_cast<const char*>(&asrc->pitch),       sizeof(asrc->pitch));
            file.write(reinterpret_cast<const char*>(&asrc->minDistance), sizeof(asrc->minDistance));
            file.write(reinterpret_cast<const char*>(&asrc->maxDistance), sizeof(asrc->maxDistance));
            uint8_t f = (uint8_t)((asrc->loop ? 1u : 0u) | (asrc->spatial ? 2u : 0u) | (asrc->playOnStart ? 4u : 0u));
            file.write(reinterpret_cast<const char*>(&f), sizeof(f));
        }

        // v11: animator.
        const AnimatorComponent* anim = reg.try_get<AnimatorComponent>(e);
        uint8_t hasAnim = anim ? 1u : 0u;
        file.write(reinterpret_cast<const char*>(&hasAnim), sizeof(hasAnim));
        if (anim) {
            int32_t clip  = anim->clip;
            uint8_t flags = (uint8_t)((anim->loop ? 1u : 0u) | (anim->playing ? 2u : 0u));
            file.write(reinterpret_cast<const char*>(&clip),        sizeof(clip));
            file.write(reinterpret_cast<const char*>(&anim->speed), sizeof(anim->speed));
            file.write(reinterpret_cast<const char*>(&flags),       sizeof(flags));
        }
    }

    return true;
}

bool SceneSerializer::Load(const std::string& path, Scene& scene) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    char magic[4];
    if (!file.read(magic, 4)) return false;
    if (std::memcmp(magic, kMagic, 4) != 0) return false;

    uint32_t version = 0;
    if (!file.read(reinterpret_cast<char*>(&version), sizeof(version))) return false;
    if (version < 4 || version > 11) return false;  // v4 pre-light .. v10 audio, v11 animator

    Guid sceneGuid;
    if (!file.read(reinterpret_cast<char*>(sceneGuid.bytes.data()), sceneGuid.bytes.size())) return false;

    uint32_t count = 0;
    if (!file.read(reinterpret_cast<char*>(&count), sizeof(count))) return false;

    scene.Clear();
    entt::registry& reg = scene.Reg();

    std::vector<entt::entity> byIndex;     // file index -> created entity
    std::vector<int32_t>      parentIdx;   // file index -> parent file index (-1 = root)
    byIndex.reserve(count);
    parentIdx.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        std::string name;
        if (!ReadString(file, name)) return false;

        uint32_t typeI = 0;
        if (!file.read(reinterpret_cast<char*>(&typeI), sizeof(typeI))) return false;
        PrimitiveType type = static_cast<PrimitiveType>(typeI);

        std::string modelPath, materialPath;
        if (!ReadString(file, modelPath))    return false;
        if (!ReadString(file, materialPath)) return false;

        float aabbMin[3], aabbMax[3], position[3], rotQuat[4], scale[3];
        if (!file.read(reinterpret_cast<char*>(aabbMin),  sizeof(aabbMin)))  return false;
        if (!file.read(reinterpret_cast<char*>(aabbMax),  sizeof(aabbMax)))  return false;
        if (!file.read(reinterpret_cast<char*>(position), sizeof(position))) return false;
        if (!file.read(reinterpret_cast<char*>(rotQuat),  sizeof(rotQuat)))  return false;
        if (!file.read(reinterpret_cast<char*>(scale),    sizeof(scale)))    return false;

        int   lightType = 0;
        float lightColor[3] = { 1.0f, 1.0f, 1.0f };
        float lightIntensity = 3.0f, lightRange = 10.0f, spotInnerDeg = 25.0f, spotOuterDeg = 35.0f;
        if (version >= 5) {
            if (!file.read(reinterpret_cast<char*>(&lightType),      sizeof(lightType)))      return false;
            if (!file.read(reinterpret_cast<char*>(lightColor),      sizeof(lightColor)))      return false;
            if (!file.read(reinterpret_cast<char*>(&lightIntensity), sizeof(lightIntensity))) return false;
            if (!file.read(reinterpret_cast<char*>(&lightRange),     sizeof(lightRange)))      return false;
            if (!file.read(reinterpret_cast<char*>(&spotInnerDeg),   sizeof(spotInnerDeg)))    return false;
            if (!file.read(reinterpret_cast<char*>(&spotOuterDeg),   sizeof(spotOuterDeg)))    return false;
        }

        // Build the entity from the decoded record.
        entt::entity e = scene.Create(name);
        byIndex.push_back(e);
        TransformComponent& tr = reg.get<TransformComponent>(e);
        tr.position = glm::vec3(position[0], position[1], position[2]);
        tr.rotation = glm::quat(rotQuat[3], rotQuat[0], rotQuat[1], rotQuat[2]);   // w,x,y,z
        tr.scale    = glm::vec3(scale[0], scale[1], scale[2]);

        MeshKind mk = MeshKindFromPrimitive(type);
        if (mk != MeshKind::None) {
            MeshComponent mc;
            mc.kind      = mk;
            mc.modelPath = modelPath;
            mc.aabbMin   = glm::vec3(aabbMin[0], aabbMin[1], aabbMin[2]);
            mc.aabbMax   = glm::vec3(aabbMax[0], aabbMax[1], aabbMax[2]);
            reg.emplace<MeshComponent>(e, mc);
        }
        if (!materialPath.empty())
            reg.emplace<MaterialComponent>(e, MaterialComponent{ materialPath });
        if (type == PrimitiveType::Light) {
            LightComponent lc;
            lc.type      = lightType;
            lc.color     = glm::vec3(lightColor[0], lightColor[1], lightColor[2]);
            lc.intensity = lightIntensity;
            lc.range     = lightRange;
            lc.innerDeg  = spotInnerDeg;
            lc.outerDeg  = spotOuterDeg;
            reg.emplace<LightComponent>(e, lc);
        }
        if (type == PrimitiveType::Camera)
            reg.emplace<CameraComponent>(e);

        if (version >= 6) {
            uint32_t ccount = 0;
            if (!file.read(reinterpret_cast<char*>(&ccount), sizeof(ccount))) return false;
            for (uint32_t c = 0; c < ccount; ++c) {
                uint32_t ctype = 0;
                bool     enabled = true;
                float    axis[3] = { 0, 1, 0 };
                float    speed = 0.0f, amount = 0.0f;
                std::string scriptPath;
                if (!file.read(reinterpret_cast<char*>(&ctype),   sizeof(ctype)))   return false;
                if (!file.read(reinterpret_cast<char*>(&enabled), sizeof(enabled))) return false;
                if (!file.read(reinterpret_cast<char*>(axis),     sizeof(axis)))    return false;
                if (!file.read(reinterpret_cast<char*>(&speed),   sizeof(speed)))   return false;
                if (!file.read(reinterpret_cast<char*>(&amount),  sizeof(amount)))  return false;
                if (!ReadString(file, scriptPath)) return false;

                glm::vec3 ax(axis[0], axis[1], axis[2]);
                if (ctype == 0)
                    reg.emplace_or_replace<RotatorComponent>(e, RotatorComponent{ ax, speed, enabled });
                else if (ctype == 1)
                    reg.emplace_or_replace<FloaterComponent>(e, FloaterComponent{ ax, speed, amount, enabled });
                else if (ctype == 2)
                    reg.emplace_or_replace<ScriptComponent>(e, ScriptComponent{ scriptPath, enabled });
            }
        }

        // v7: parent link (resolved to entities in a second pass below).
        int32_t parentIndex = -1;
        if (version >= 7) {
            if (!file.read(reinterpret_cast<char*>(&parentIndex), sizeof(parentIndex))) return false;
        }

        // v8: physics components.
        if (version >= 8) {
            uint8_t physFlags = 0;
            if (!file.read(reinterpret_cast<char*>(&physFlags), sizeof(physFlags))) return false;
            if (physFlags & 1u) {
                uint32_t shape = 0; float he[3] = { 0.5f, 0.5f, 0.5f };
                float radius = 0.5f, halfHeight = 0.5f;
                if (!file.read(reinterpret_cast<char*>(&shape), sizeof(shape)))           return false;
                if (!file.read(reinterpret_cast<char*>(he), sizeof(he)))                  return false;
                if (!file.read(reinterpret_cast<char*>(&radius), sizeof(radius)))         return false;
                if (!file.read(reinterpret_cast<char*>(&halfHeight), sizeof(halfHeight))) return false;
                ColliderComponent cc;
                cc.shape       = (ColliderShape)shape;
                cc.halfExtents = glm::vec3(he[0], he[1], he[2]);
                cc.radius      = radius;
                cc.halfHeight  = halfHeight;
                reg.emplace<ColliderComponent>(e, cc);
            }
            if (physFlags & 2u) {
                uint32_t bt = 0; float mass = 1.0f, friction = 0.5f, rest = 0.1f; uint8_t awake = 1;
                if (!file.read(reinterpret_cast<char*>(&bt), sizeof(bt)))             return false;
                if (!file.read(reinterpret_cast<char*>(&mass), sizeof(mass)))         return false;
                if (!file.read(reinterpret_cast<char*>(&friction), sizeof(friction))) return false;
                if (!file.read(reinterpret_cast<char*>(&rest), sizeof(rest)))         return false;
                if (!file.read(reinterpret_cast<char*>(&awake), sizeof(awake)))       return false;
                RigidBodyComponent rb;
                rb.type        = (BodyType)bt;
                rb.mass        = mass;
                rb.friction    = friction;
                rb.restitution = rest;
                rb.startAwake  = awake != 0;
                reg.emplace<RigidBodyComponent>(e, rb);
            }
        }

        // v9: character controller.
        if (version >= 9) {
            uint8_t hasChar = 0;
            if (!file.read(reinterpret_cast<char*>(&hasChar), sizeof(hasChar))) return false;
            if (hasChar) {
                CharacterControllerComponent chc;
                uint32_t vw = 0;
                if (!file.read(reinterpret_cast<char*>(&chc.height),           sizeof(chc.height)))           return false;
                if (!file.read(reinterpret_cast<char*>(&chc.radius),           sizeof(chc.radius)))           return false;
                if (!file.read(reinterpret_cast<char*>(&chc.moveSpeed),        sizeof(chc.moveSpeed)))        return false;
                if (!file.read(reinterpret_cast<char*>(&chc.jumpSpeed),        sizeof(chc.jumpSpeed)))        return false;
                if (!file.read(reinterpret_cast<char*>(&chc.eyeHeight),        sizeof(chc.eyeHeight)))        return false;
                if (!file.read(reinterpret_cast<char*>(&chc.thirdDistance),    sizeof(chc.thirdDistance)))    return false;
                if (!file.read(reinterpret_cast<char*>(&chc.mouseSensitivity), sizeof(chc.mouseSensitivity))) return false;
                if (!file.read(reinterpret_cast<char*>(&vw),                   sizeof(vw)))                   return false;
                chc.view = (CameraView)vw;
                reg.emplace<CharacterControllerComponent>(e, chc);
            }
        }

        // v10: audio source + listener.
        if (version >= 10) {
            uint8_t audioFlags = 0;
            if (!file.read(reinterpret_cast<char*>(&audioFlags), sizeof(audioFlags))) return false;
            if (audioFlags & 1u) {
                AudioSourceComponent as;
                if (!ReadString(file, as.clip)) return false;
                if (!file.read(reinterpret_cast<char*>(&as.volume),      sizeof(as.volume)))      return false;
                if (!file.read(reinterpret_cast<char*>(&as.pitch),       sizeof(as.pitch)))       return false;
                if (!file.read(reinterpret_cast<char*>(&as.minDistance), sizeof(as.minDistance))) return false;
                if (!file.read(reinterpret_cast<char*>(&as.maxDistance), sizeof(as.maxDistance))) return false;
                uint8_t f = 0;
                if (!file.read(reinterpret_cast<char*>(&f), sizeof(f))) return false;
                as.loop = (f & 1u) != 0; as.spatial = (f & 2u) != 0; as.playOnStart = (f & 4u) != 0;
                reg.emplace<AudioSourceComponent>(e, as);
            }
            if (audioFlags & 2u)
                reg.emplace<AudioListenerComponent>(e, AudioListenerComponent{});
        }

        // v11: animator.
        if (version >= 11) {
            uint8_t hasAnim = 0;
            if (!file.read(reinterpret_cast<char*>(&hasAnim), sizeof(hasAnim))) return false;
            if (hasAnim) {
                AnimatorComponent an;
                int32_t clip = 0; uint8_t flags = 0;
                if (!file.read(reinterpret_cast<char*>(&clip),      sizeof(clip)))      return false;
                if (!file.read(reinterpret_cast<char*>(&an.speed),  sizeof(an.speed)))  return false;
                if (!file.read(reinterpret_cast<char*>(&flags),     sizeof(flags)))     return false;
                an.clip = clip; an.loop = (flags & 1u) != 0; an.playing = (flags & 2u) != 0; an.time = 0.0f;
                reg.emplace<AnimatorComponent>(e, an);
            }
        }
        parentIdx.push_back(parentIndex);
    }

    // Second pass: every entity now exists, so link children to their parents.
    for (uint32_t i = 0; i < byIndex.size(); ++i) {
        int32_t p = parentIdx[i];
        if (p >= 0 && static_cast<uint32_t>(p) < byIndex.size())
            reg.emplace<ParentComponent>(byIndex[i], ParentComponent{ byIndex[static_cast<uint32_t>(p)] });
    }

    return true;
}
