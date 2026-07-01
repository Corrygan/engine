#include "LuaScripting.h"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "../Physics/PhysicsWorld.h"
#include "../Audio/AudioEngine.h"
#include <tuple>
#include <vector>
#include <algorithm>

namespace {
    struct LuaEntity {
        entt::registry* reg = nullptr;
        entt::entity    e   = entt::null;

        TransformComponent* xf() const {
            if (!reg || !reg->valid(e)) return nullptr;
            return reg->try_get<TransformComponent>(e);
        }
        std::string name() const {
            if (!reg || !reg->valid(e)) return std::string();
            const auto* n = reg->try_get<NameComponent>(e);
            return n ? n->name : std::string();
        }
    };

    glm::vec3 EulerDeg(const TransformComponent& t) {
        return glm::degrees(glm::eulerAngles(t.rotation));
    }
    void SetEulerDeg(TransformComponent& t, float x, float y, float z) {
        t.rotation = glm::quat(glm::radians(glm::vec3(x, y, z)));
    }
}

struct LuaScripting::Impl {
    sol::state lua;
    struct Inst { entt::entity e; sol::environment env; };
    std::vector<Inst> instances;

    Scene*                    scene = nullptr;     // active scene during play
    std::vector<entt::entity> pendingDestroy;      // deferred (applied after update)
    float                     elapsed = 0.0f;      // seconds since Play started
    float                     lastDt  = 0.0f;
};

LuaScripting::LuaScripting() {
    m_impl = new Impl();
    sol::state& lua = m_impl->lua;
    lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);

    lua.set_function("log", [this](sol::object v) {
        Log(v.is<std::string>() ? v.as<std::string>()
                                : sol::state_view(v.lua_state())["tostring"](v).get<std::string>());
    });

    lua.new_usertype<LuaEntity>("Entity",
        "getName", [](LuaEntity& e) { return e.name(); },
        "getPos",  [](LuaEntity& e) {
            auto* t = e.xf();
            if (!t) return std::make_tuple(0.0f, 0.0f, 0.0f);
            return std::make_tuple(t->position.x, t->position.y, t->position.z);
        },
        "setPos",  [](LuaEntity& e, float x, float y, float z) {
            if (auto* t = e.xf()) t->position = glm::vec3(x, y, z);
        },
        "translate", [](LuaEntity& e, float x, float y, float z) {
            if (auto* t = e.xf()) t->position += glm::vec3(x, y, z);
        },
        "getRotation", [](LuaEntity& e) {
            auto* t = e.xf();
            if (!t) return std::make_tuple(0.0f, 0.0f, 0.0f);
            glm::vec3 e3 = EulerDeg(*t);
            return std::make_tuple(e3.x, e3.y, e3.z);
        },
        "setRotation", [](LuaEntity& e, float x, float y, float z) {
            if (auto* t = e.xf()) SetEulerDeg(*t, x, y, z);
        },
        "rotate", [](LuaEntity& e, float x, float y, float z) {
            if (auto* t = e.xf()) { glm::vec3 c = EulerDeg(*t); SetEulerDeg(*t, c.x+x, c.y+y, c.z+z); }
        },
        "getScale", [](LuaEntity& e) {
            auto* t = e.xf();
            if (!t) return std::make_tuple(1.0f, 1.0f, 1.0f);
            return std::make_tuple(t->scale.x, t->scale.y, t->scale.z);
        },
        "setScale", [](LuaEntity& e, float x, float y, float z) {
            if (auto* t = e.xf()) t->scale = glm::vec3(x, y, z);
        },
        "forward", [](LuaEntity& e) {
            auto* t = e.xf();
            glm::vec3 f = t ? glm::normalize(t->rotation * glm::vec3(0, 0, -1)) : glm::vec3(0, 0, -1);
            return std::make_tuple(f.x, f.y, f.z);
        },
        "right", [](LuaEntity& e) {
            auto* t = e.xf();
            glm::vec3 r = t ? glm::normalize(t->rotation * glm::vec3(1, 0, 0)) : glm::vec3(1, 0, 0);
            return std::make_tuple(r.x, r.y, r.z);
        },
        "up", [](LuaEntity& e) {
            auto* t = e.xf();
            glm::vec3 u = t ? glm::normalize(t->rotation * glm::vec3(0, 1, 0)) : glm::vec3(0, 1, 0);
            return std::make_tuple(u.x, u.y, u.z);
        },
        "isValid", [](LuaEntity& e) { return e.reg && e.reg->valid(e.e); },
        "destroy", [this](LuaEntity& e) {
            if (e.reg && e.reg->valid(e.e)) m_impl->pendingDestroy.push_back(e.e);
        },
        "applyImpulse", [this](LuaEntity& e, float x, float y, float z) {
            if (m_physics && e.reg && e.reg->valid(e.e)) m_physics->ApplyImpulse(e.e, glm::vec3(x, y, z));
        },
        "setVelocity", [this](LuaEntity& e, float x, float y, float z) {
            if (m_physics && e.reg && e.reg->valid(e.e)) m_physics->SetVelocity(e.e, glm::vec3(x, y, z));
        },
        "getVelocity", [this](LuaEntity& e) {
            glm::vec3 v(0.0f);
            if (m_physics && e.reg && e.reg->valid(e.e)) v = m_physics->GetVelocity(e.e);
            return std::make_tuple(v.x, v.y, v.z);
        },
        "isGrounded", [this](LuaEntity&) { return m_physics && m_physics->CharacterGrounded(); }
    );

    // ── scene: find / spawn / destroy entities ───────────────────────────────
    sol::table sceneT = lua.create_named_table("scene");
    sceneT.set_function("find", [this](const std::string& name) -> sol::object {
        if (!m_impl->scene) return sol::lua_nil;
        entt::registry& reg = m_impl->scene->Reg();
        for (entt::entity e : reg.view<NameComponent>())
            if (reg.get<NameComponent>(e).name == name)
                return sol::make_object(m_impl->lua, LuaEntity{ &reg, e });
        return sol::lua_nil;
    });
    sceneT.set_function("spawn", [this](const std::string& name) -> sol::object {
        if (!m_impl->scene) return sol::lua_nil;
        entt::entity e = m_impl->scene->Create(name);
        return sol::make_object(m_impl->lua, LuaEntity{ &m_impl->scene->Reg(), e });
    });
    sceneT.set_function("destroy", [this](LuaEntity& e) {
        if (e.reg && e.reg->valid(e.e)) m_impl->pendingDestroy.push_back(e.e);
    });
    sceneT.set_function("count", [this]() {
        return m_impl->scene ? (int)m_impl->scene->Reg().view<NameComponent>().size() : 0;
    });

    // ── time ─────────────────────────────────────────────────────────────────
    sol::table timeT = lua.create_named_table("time");
    timeT.set_function("elapsed", [this]() { return m_impl->elapsed; });
    timeT.set_function("dt",      [this]() { return m_impl->lastDt; });

    // ── physics: dynamics control + raycast ──────────────────────────────────
    sol::table physT = lua.create_named_table("physics");
    physT.set_function("applyImpulse", [this](LuaEntity& e, float x, float y, float z) {
        if (m_physics && e.reg && e.reg->valid(e.e)) m_physics->ApplyImpulse(e.e, glm::vec3(x, y, z));
    });
    physT.set_function("applyForce", [this](LuaEntity& e, float x, float y, float z) {
        if (m_physics && e.reg && e.reg->valid(e.e)) m_physics->ApplyForce(e.e, glm::vec3(x, y, z));
    });
    physT.set_function("setVelocity", [this](LuaEntity& e, float x, float y, float z) {
        if (m_physics && e.reg && e.reg->valid(e.e)) m_physics->SetVelocity(e.e, glm::vec3(x, y, z));
    });
    physT.set_function("getVelocity", [this](LuaEntity& e) {
        glm::vec3 v(0.0f);
        if (m_physics && e.reg && e.reg->valid(e.e)) v = m_physics->GetVelocity(e.e);
        return std::make_tuple(v.x, v.y, v.z);
    });
    physT.set_function("grounded", [this]() { return m_physics && m_physics->CharacterGrounded(); });
    physT.set_function("raycast",
        [this](float ox, float oy, float oz, float dx, float dy, float dz, float maxDist) -> sol::table {
            sol::table t = m_impl->lua.create_table();
            RaycastHit h;
            if (m_physics) {
                glm::vec3 d(dx, dy, dz);
                if (glm::length(d) > 1e-6f) d = glm::normalize(d);
                h = m_physics->Raycast(glm::vec3(ox, oy, oz), d, maxDist);
            }
            t["hit"]      = h.hit;
            t["x"]        = h.point.x;
            t["y"]        = h.point.y;
            t["z"]        = h.point.z;
            t["distance"] = h.distance;
            if (h.hit && h.entity != entt::null && m_impl->scene)
                t["entity"] = sol::make_object(m_impl->lua, LuaEntity{ &m_impl->scene->Reg(), h.entity });
            return t;
        });

    // ── input (provided by the editor) ────────────────────────────────────────
    sol::table inputT = lua.create_named_table("input");
    inputT.set_function("keyDown", [this](const std::string& k) {
        return m_input.keyDown ? m_input.keyDown(k) : false;
    });
    inputT.set_function("mouseButton", [this](int b) {
        return m_input.mouseButton ? m_input.mouseButton(b) : false;
    });
    inputT.set_function("mouseDelta", [this]() {
        float x = 0.0f, y = 0.0f; if (m_input.mouseDelta) m_input.mouseDelta(x, y);
        return std::make_tuple(x, y);
    });
    inputT.set_function("mousePos", [this]() {
        float x = 0.0f, y = 0.0f; if (m_input.mousePos) m_input.mousePos(x, y);
        return std::make_tuple(x, y);
    });

    // ── audio ─────────────────────────────────────────────────────────────────
    sol::table audioT = lua.create_named_table("audio");
    audioT.set_function("play", [this](const std::string& clip) {
        if (m_audio) m_audio->Play(clip, 1.0f);
    });
    audioT.set_function("playAt", [this](const std::string& clip, float x, float y, float z) {
        if (m_audio) m_audio->PlayAt(clip, glm::vec3(x, y, z), 1.0f);
    });
    audioT.set_function("playSource", [this](LuaEntity& e) {
        if (m_audio && m_impl->scene && e.reg && e.reg->valid(e.e))
            m_audio->PlaySource(*m_impl->scene, e.e);
    });
    audioT.set_function("busVolume", [this](int bus, float v) {
        if (m_audio && bus >= 0 && bus < 3) m_audio->SetBusVolume((AudioBus)bus, v);
    });

    // ── animation ─────────────────────────────────────────────────────────────
    sol::table animT = lua.create_named_table("anim");
    animT.set_function("play", [](LuaEntity& e, int clip) {
        if (!e.reg || !e.reg->valid(e.e)) return;
        AnimatorComponent a;
        a.clip = clip; a.time = 0.0f; a.playing = true;
        if (auto* cur = e.reg->try_get<AnimatorComponent>(e.e)) { a.speed = cur->speed; a.loop = cur->loop; }
        e.reg->emplace_or_replace<AnimatorComponent>(e.e, a);
    });
    animT.set_function("stop", [](LuaEntity& e) {
        if (e.reg && e.reg->valid(e.e))
            if (auto* a = e.reg->try_get<AnimatorComponent>(e.e)) a->playing = false;
    });
    animT.set_function("setSpeed", [](LuaEntity& e, float s) {
        if (e.reg && e.reg->valid(e.e))
            if (auto* a = e.reg->try_get<AnimatorComponent>(e.e)) a->speed = s;
    });
}

LuaScripting::~LuaScripting() {
    delete m_impl;
}

void LuaScripting::Log(const std::string& msg) {
    if (m_log) m_log(msg);
}

void LuaScripting::StartAll(Scene& scene) {
    StopAll();
    sol::state& lua = m_impl->lua;
    entt::registry& reg = scene.Reg();
    m_impl->scene   = &scene;
    m_impl->elapsed = 0.0f;
    m_impl->pendingDestroy.clear();

    auto scripts = reg.view<ScriptComponent>();
    for (entt::entity e : scripts) {
        const ScriptComponent& sc = scripts.get<ScriptComponent>(e);
        if (!sc.enabled || sc.path.empty()) continue;

        sol::environment env(lua, sol::create, lua.globals());
        env["self"] = LuaEntity{ &reg, e };

        auto res = lua.safe_script_file(sc.path, env, &sol::script_pass_on_error);
        if (!res.valid()) {
            sol::error err = res;
            Log(std::string("[Lua] load error: ") + err.what());
            continue;
        }

        sol::protected_function onStart = env["OnStart"];
        if (onStart.valid()) {
            auto sr = onStart();
            if (!sr.valid()) { sol::error er = sr; Log(std::string("[Lua] OnStart: ") + er.what()); }
        }
        m_impl->instances.push_back({ e, std::move(env) });
    }
}

void LuaScripting::UpdateAll(Scene& scene, float dt) {
    m_impl->scene    = &scene;
    m_impl->lastDt   = dt;
    m_impl->elapsed += dt;
    entt::registry& reg = scene.Reg();

    for (auto& inst : m_impl->instances) {
        if (!reg.valid(inst.e)) continue;          // entity was destroyed: skip
        sol::protected_function upd = inst.env["OnUpdate"];
        if (!upd.valid()) continue;
        auto r = upd(dt);
        if (!r.valid()) { sol::error e = r; Log(std::string("[Lua] OnUpdate: ") + e.what()); }
    }

    // Apply deferred destroys, then drop instances whose entity is gone.
    if (!m_impl->pendingDestroy.empty()) {
        for (entt::entity e : m_impl->pendingDestroy)
            if (reg.valid(e)) scene.Destroy(e);
        m_impl->pendingDestroy.clear();
    }
    m_impl->instances.erase(
        std::remove_if(m_impl->instances.begin(), m_impl->instances.end(),
            [&](const Impl::Inst& i) { return !reg.valid(i.e); }),
        m_impl->instances.end());
}

void LuaScripting::StopAll() {
    for (auto& inst : m_impl->instances) {
        sol::protected_function od = inst.env["OnDestroy"];
        if (od.valid()) od();
    }
    m_impl->instances.clear();
}
