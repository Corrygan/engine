#pragma once
#include <string>
#include <functional>
#include "../Scene/Scene.h"

// Drives Script components: per-entity Lua environment with OnStart/OnUpdate/
// OnDestroy callbacks and an engine API (self:getPos, log, ...). sol2 is kept
// out of this header via pimpl so only one TU pays the compile cost.
class LuaScripting {
public:
    LuaScripting();
    ~LuaScripting();

    void SetLogCallback(std::function<void(const std::string&)> cb) { m_log = std::move(cb); }

    void StartAll(Scene& scene);                  // on Play
    void UpdateAll(Scene& scene, float dt);       // each frame
    void StopAll();                               // on Stop

private:
    void Log(const std::string& msg);

    struct Impl;
    Impl* m_impl = nullptr;
    std::function<void(const std::string&)> m_log;
};
