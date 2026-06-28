#include "PhysicsWorld.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <unordered_map>
#include <algorithm>
#include <thread>
#include <cmath>

// ── Jolt layer setup (the standard two-layer NON_MOVING / MOVING scheme) ─────
namespace {
namespace Layers {
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING     = 1;
    static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
}
namespace BroadPhaseLayers {
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
    static constexpr JPH::uint            NUM_LAYERS(2);
}

class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    BPLayerInterfaceImpl() {
        m_map[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        m_map[Layers::MOVING]     = BroadPhaseLayers::MOVING;
    }
    JPH::uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
        return m_map[inLayer];
    }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer) const override { return "Layer"; }
#endif
private:
    JPH::BroadPhaseLayer m_map[Layers::NUM_LAYERS];
};

class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer layer1, JPH::BroadPhaseLayer layer2) const override {
        switch (layer1) {
            case Layers::NON_MOVING: return layer2 == BroadPhaseLayers::MOVING;
            case Layers::MOVING:     return true;
            default:                 return false;
        }
    }
};

class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer o1, JPH::ObjectLayer o2) const override {
        switch (o1) {
            case Layers::NON_MOVING: return o2 == Layers::MOVING;  // static collides only with moving
            case Layers::MOVING:     return true;
            default:                 return false;
        }
    }
};

// One-time global Jolt initialization (allocator + factory + type registration).
void EnsureJoltInit() {
    static bool done = false;
    if (done) return;
    done = true;
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();
}

void DecomposeTRS(const glm::mat4& m, glm::vec3& pos, glm::quat& rot, glm::vec3& scale) {
    pos   = glm::vec3(m[3]);
    scale = glm::vec3(glm::length(glm::vec3(m[0])),
                      glm::length(glm::vec3(m[1])),
                      glm::length(glm::vec3(m[2])));
    glm::mat3 r(glm::vec3(m[0]) / (scale.x > 1e-6f ? scale.x : 1.0f),
                glm::vec3(m[1]) / (scale.y > 1e-6f ? scale.y : 1.0f),
                glm::vec3(m[2]) / (scale.z > 1e-6f ? scale.z : 1.0f));
    rot = glm::quat_cast(r);
}

// Build a Jolt shape for a collider, baking in the entity's world scale.
JPH::Ref<JPH::Shape> MakeShape(const ColliderComponent& col, const glm::vec3& scale) {
    glm::vec3 s = glm::abs(scale);
    switch (col.shape) {
        case ColliderShape::Sphere: {
            float r = col.radius * std::max({ s.x, s.y, s.z });
            return new JPH::SphereShape(std::max(r, 0.05f));
        }
        case ColliderShape::Capsule: {
            float r  = col.radius     * std::max(s.x, s.z);
            float hh = col.halfHeight * s.y;
            return new JPH::CapsuleShape(std::max(hh, 0.05f), std::max(r, 0.05f));
        }
        case ColliderShape::Box:
        default: {
            glm::vec3 he = glm::max(col.halfExtents * s, glm::vec3(0.05f));
            return new JPH::BoxShape(JPH::Vec3(he.x, he.y, he.z));
        }
    }
}
} // namespace

// ── Impl ─────────────────────────────────────────────────────────────────────
struct PhysicsWorld::Impl {
    JPH::TempAllocatorImpl   tempAllocator{ 16 * 1024 * 1024 };
    JPH::JobSystemThreadPool jobSystem{ JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
        std::max(1, (int)std::thread::hardware_concurrency() - 1) };
    BPLayerInterfaceImpl              broadphaseLayers;
    ObjectVsBroadPhaseLayerFilterImpl objectVsBroadphase;
    ObjectLayerPairFilterImpl         objectPairFilter;
    JPH::PhysicsSystem                system;
    std::unordered_map<entt::entity, JPH::BodyID> bodies;
    bool active = false;

    JPH::CharacterVirtual* character   = nullptr;
    entt::entity           charEntity  = entt::null;
    float                  charSpeed   = 5.0f;
    float                  charJump    = 5.0f;
    float                  charHeight  = 1.8f;
    glm::vec3              moveInput{ 0.0f };
    bool                   jumpInput   = false;
};

PhysicsWorld::PhysicsWorld() {
    EnsureJoltInit();
    m_impl = std::make_unique<Impl>();
    m_impl->system.Init(4096, 0, 4096, 2048,
                        m_impl->broadphaseLayers,
                        m_impl->objectVsBroadphase,
                        m_impl->objectPairFilter);
    m_impl->system.SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));

    // Jolt discards restitution for impacts slower than this (default 1.0 m/s) to
    // keep resting bodies from jittering. That's high enough to make ordinary
    // drops look dead, so lower it — bounces become visible while stacks stay calm.
    JPH::PhysicsSettings ps = m_impl->system.GetPhysicsSettings();
    ps.mMinVelocityForRestitution = 0.1f;
    m_impl->system.SetPhysicsSettings(ps);
}

PhysicsWorld::~PhysicsWorld() { End(); }

bool PhysicsWorld::Active() const { return m_impl->active; }

void PhysicsWorld::Begin(Scene& scene) {
    if (m_impl->active) End();

    entt::registry& reg = scene.Reg();
    UpdateWorldTransforms(reg);
    JPH::BodyInterface& bi = m_impl->system.GetBodyInterface();

    for (entt::entity e : reg.view<ColliderComponent>()) {
        if (reg.all_of<CharacterControllerComponent>(e)) continue;  // character collides on its own
        const ColliderComponent&  col = reg.get<ColliderComponent>(e);
        const RigidBodyComponent* rb  = reg.try_get<RigidBodyComponent>(e);

        glm::vec3 pos, scl; glm::quat rot;
        DecomposeTRS(WorldMatrixOf(reg, e), pos, rot, scl);

        JPH::Ref<JPH::Shape> shape = MakeShape(col, scl);
        if (!shape) continue;

        JPH::EMotionType motion = JPH::EMotionType::Static;
        JPH::ObjectLayer layer  = Layers::NON_MOVING;
        if (rb && rb->type == BodyType::Dynamic)   { motion = JPH::EMotionType::Dynamic;   layer = Layers::MOVING; }
        if (rb && rb->type == BodyType::Kinematic) { motion = JPH::EMotionType::Kinematic; layer = Layers::MOVING; }

        JPH::BodyCreationSettings bcs(shape.GetPtr(),
            JPH::RVec3(pos.x, pos.y, pos.z),
            JPH::Quat(rot.x, rot.y, rot.z, rot.w),
            motion, layer);
        if (rb) {
            bcs.mFriction    = rb->friction;
            bcs.mRestitution = rb->restitution;
            if (rb->type == BodyType::Dynamic) {
                bcs.mOverrideMassProperties     = JPH::EOverrideMassProperties::CalculateInertia;
                bcs.mMassPropertiesOverride.mMass = std::max(rb->mass, 0.001f);
            }
        }

        JPH::Body* body = bi.CreateBody(bcs);
        if (!body) continue;
        JPH::EActivation act = (motion == JPH::EMotionType::Dynamic && (!rb || rb->startAwake))
                             ? JPH::EActivation::Activate : JPH::EActivation::DontActivate;
        bi.AddBody(body->GetID(), act);
        m_impl->bodies[e] = body->GetID();
    }

    m_impl->system.OptimizeBroadPhase();

    // Character controller — the first entity with the component (one for v1).
    for (entt::entity e : reg.view<CharacterControllerComponent>()) {
        const CharacterControllerComponent& cc = reg.get<CharacterControllerComponent>(e);
        glm::vec3 pos, scl; glm::quat rot;
        DecomposeTRS(WorldMatrixOf(reg, e), pos, rot, scl);
        float radius  = std::max(0.05f, cc.radius);
        float halfCyl = std::max(0.0f, cc.height * 0.5f - radius);

        JPH::CharacterVirtualSettings settings;
        settings.mShape            = new JPH::CapsuleShape(halfCyl, radius);
        settings.mUp               = JPH::Vec3::sAxisY();
        settings.mMaxSlopeAngle    = JPH::DegreesToRadians(46.0f);
        settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -radius);

        m_impl->character  = new JPH::CharacterVirtual(&settings,
            JPH::RVec3(pos.x, pos.y, pos.z), JPH::Quat::sIdentity(), 0, &m_impl->system);
        m_impl->charEntity = e;
        m_impl->charHeight = cc.height;
        m_impl->charSpeed  = cc.moveSpeed;
        m_impl->charJump   = cc.jumpSpeed;
        break;
    }

    m_impl->active = true;
}

void PhysicsWorld::Step(Scene& scene, float dt) {
    if (!m_impl->active || dt <= 0.0f) return;
    dt = std::min(dt, 1.0f / 30.0f);   // clamp big hitches so the sim stays stable

    // More collision substeps for big frame deltas so fast impacts resolve (and
    // bounce) accurately instead of being smeared across one large step.
    int steps = (dt > 1.0f / 55.0f) ? 2 : 1;
    m_impl->system.Update(dt, steps, &m_impl->tempAllocator, &m_impl->jobSystem);

    entt::registry&     reg = scene.Reg();
    JPH::BodyInterface& bi  = m_impl->system.GetBodyInterface();
    for (auto& [e, id] : m_impl->bodies) {
        if (!reg.valid(e)) continue;
        const RigidBodyComponent* rb = reg.try_get<RigidBodyComponent>(e);
        if (!rb || rb->type == BodyType::Static) continue;   // static bodies never move

        JPH::RVec3 p = bi.GetPosition(id);
        JPH::Quat  q = bi.GetRotation(id);
        glm::vec3  worldPos(p.GetX(), p.GetY(), p.GetZ());
        glm::quat  worldRot(q.GetW(), q.GetX(), q.GetY(), q.GetZ());   // glm: w,x,y,z

        TransformComponent& tr = reg.get<TransformComponent>(e);
        const ParentComponent* pc = reg.try_get<ParentComponent>(e);
        if (pc && pc->parent != entt::null && reg.valid(pc->parent)) {
            // Body simulates in world space; convert back to parent-local.
            glm::mat4 world = glm::translate(glm::mat4(1.0f), worldPos) * glm::mat4_cast(worldRot);
            glm::mat4 local = glm::inverse(WorldMatrixOf(reg, pc->parent)) * world;
            glm::vec3 lpos, lscl; glm::quat lrot;
            DecomposeTRS(local, lpos, lrot, lscl);
            tr.position = lpos;
            tr.rotation = lrot;
        } else {
            tr.position = worldPos;
            tr.rotation = worldRot;
        }
    }

    // Character controller (after the rigid-body step so it collides with the
    // up-to-date world). Gravity + jump on the vertical axis, input drives the
    // horizontal; ExtendedUpdate walks stairs / slides on slopes / sticks to floor.
    if (m_impl->character) {
        JPH::CharacterVirtual* ch = m_impl->character;
        bool grounded = ch->GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;

        float vy = ch->GetLinearVelocity().GetY();
        if (grounded && vy < 0.0f)          vy = 0.0f;             // settle on the ground
        if (grounded && m_impl->jumpInput)  vy = m_impl->charJump; // jump
        if (!grounded)                      vy += -9.81f * dt;     // gravity while airborne

        glm::vec3 mv = m_impl->moveInput;
        ch->SetLinearVelocity(JPH::Vec3(mv.x * m_impl->charSpeed, vy, mv.z * m_impl->charSpeed));

        JPH::CharacterVirtual::ExtendedUpdateSettings us;
        ch->ExtendedUpdate(dt, m_impl->system.GetGravity(), us,
            m_impl->system.GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
            m_impl->system.GetDefaultLayerFilter(Layers::MOVING),
            {}, {}, m_impl->tempAllocator);
        m_impl->jumpInput = false;

        if (reg.valid(m_impl->charEntity)) {
            JPH::RVec3 p = ch->GetPosition();
            reg.get<TransformComponent>(m_impl->charEntity).position =
                glm::vec3(p.GetX(), p.GetY(), p.GetZ());
        }
    }
}

void PhysicsWorld::End() {
    if (!m_impl || !m_impl->active) return;
    JPH::BodyInterface& bi = m_impl->system.GetBodyInterface();
    for (auto& [e, id] : m_impl->bodies) {
        bi.RemoveBody(id);
        bi.DestroyBody(id);
    }
    m_impl->bodies.clear();
    delete m_impl->character;
    m_impl->character  = nullptr;
    m_impl->charEntity = entt::null;
    m_impl->moveInput  = glm::vec3(0.0f);
    m_impl->jumpInput  = false;
    m_impl->active = false;
}

RaycastHit PhysicsWorld::Raycast(const glm::vec3& origin, const glm::vec3& dir, float maxDistance) const {
    RaycastHit out;
    if (!m_impl->active) return out;

    JPH::RRayCast ray{ JPH::RVec3(origin.x, origin.y, origin.z),
                       JPH::Vec3(dir.x, dir.y, dir.z) * maxDistance };
    JPH::RayCastResult result;
    if (m_impl->system.GetNarrowPhaseQuery().CastRay(ray, result)) {
        out.hit      = true;
        out.distance = result.mFraction * maxDistance;
        JPH::RVec3 hp = ray.GetPointOnRay(result.mFraction);
        out.point    = glm::vec3(hp.GetX(), hp.GetY(), hp.GetZ());
        for (auto& [e, id] : m_impl->bodies)
            if (id == result.mBodyID) { out.entity = e; break; }
    }
    return out;
}

void PhysicsWorld::SetCharacterInput(const glm::vec3& worldMoveDir, bool jump) {
    m_impl->moveInput = worldMoveDir;
    if (jump) m_impl->jumpInput = true;
}

bool PhysicsWorld::HasCharacter() const { return m_impl->character != nullptr; }
