#include "LuaScripting.h"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <tuple>

namespace {
    // Lightweight handle bound to scripts as `self`. Holds the registry + entity;
    // the registry outlives a play session, so the handle stays valid throughout.
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
};

LuaScripting::LuaScripting() {
    m_impl = new Impl();
    sol::state& lua = m_impl->lua;
    lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);

    // Engine API: console logging.
    lua.set_function("log", [this](sol::object v) {
        Log(v.is<std::string>() ? v.as<std::string>()
                                : sol::state_view(v.lua_state())["tostring"](v).get<std::string>());
    });

    // Entity handle (`self`).
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
        }
    );
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
    (void)scene;
    for (auto& inst : m_impl->instances) {
        sol::protected_function upd = inst.env["OnUpdate"];
        if (!upd.valid()) continue;
        auto r = upd(dt);
        if (!r.valid()) { sol::error e = r; Log(std::string("[Lua] OnUpdate: ") + e.what()); }
    }
}

void LuaScripting::StopAll() {
    for (auto& inst : m_impl->instances) {
        sol::protected_function od = inst.env["OnDestroy"];
        if (od.valid()) od();
    }
    m_impl->instances.clear();
}
