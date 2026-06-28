#pragma once
#include <string>
#include <functional>
#include "../Scene/Scene.h"

class PhysicsWorld;

// Drives Script components: per-entity Lua environment with OnStart/OnUpdate/
// OnDestroy callbacks and an engine API (self:getPos, log, ...). sol2 is kept
// out of this header via pimpl so only one TU pays the compile cost.
class LuaScripting {
public:
    LuaScripting();
    ~LuaScripting();

    void SetLogCallback(std::function<void(const std::string&)> cb) { m_log = std::move(cb); }
    void SetPhysics(PhysicsWorld* physics) { m_physics = physics; }   // for the Lua physics API

    // Input is supplied by the editor (mapped to ImGui) so scripting stays free
    // of any ImGui/GLFW dependency.
    struct InputProvider {
        std::function<bool(const std::string&)> keyDown;      // "w", "space", ...
        std::function<bool(int)>                mouseButton;  // 0=L, 1=R, 2=M
        std::function<void(float&, float&)>     mouseDelta;
        std::function<void(float&, float&)>     mousePos;
    };
    void SetInputProvider(InputProvider provider) { m_input = std::move(provider); }

    void StartAll(Scene& scene);                  // on Play
    void UpdateAll(Scene& scene, float dt);       // each frame
    void StopAll();                               // on Stop

private:
    void Log(const std::string& msg);

    struct Impl;
    Impl* m_impl = nullptr;
    std::function<void(const std::string&)> m_log;
    PhysicsWorld* m_physics = nullptr;
    InputProvider m_input;
};
