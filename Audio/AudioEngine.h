#pragma once
#include <glm/glm.hpp>
#include <string>
#include <memory>
#include "../Scene/Scene.h"

// Mixer buses (groups) with independent volume.
enum class AudioBus { SFX = 0, Music = 1, Voice = 2 };

// Thin wrapper over the miniaudio engine. miniaudio is hidden in the pimpl so its
// (large) header only compiles in one TU and never leaks into the rest of the
// engine. Plays our branded clips (.fcsnd/.fcmsc/.fcprs) or raw audio files.
class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    bool Init();
    void Shutdown();
    bool Ready() const;

    // Listener pose (camera / player); call once per frame while playing.
    void SetListener(const glm::vec3& pos, const glm::vec3& forward, const glm::vec3& up);

    // Fire-and-forget playback. Play = 2D (UI/music), PlayAt = 3D positioned.
    // `bus` overrides the clip's own bus when not Default.
    void Play(const std::string& clip, float volume = 1.0f);
    void PlayAt(const std::string& clip, const glm::vec3& pos, float volume = 1.0f);

    // Free finished one-shot voices; call once per frame.
    void Update();

    void  SetBusVolume(AudioBus bus, float volume);
    float BusVolume(AudioBus bus) const;

    // Scene-bound sources (Play mode). Begin starts play-on-start emitters,
    // Update syncs 3D positions + the listener (an AudioListenerComponent entity
    // if present, else the supplied camera pose), End stops them all. PlaySource
    // (re)triggers one emitter — for scripts / triggers.
    void BeginScene(Scene& scene);
    void UpdateScene(Scene& scene, const glm::vec3& camPos,
                     const glm::vec3& camForward, const glm::vec3& camUp);
    void EndScene();
    void PlaySource(Scene& scene, entt::entity e);

private:
    void PlayInternal(const std::string& clip, bool spatial, const glm::vec3& pos, float volume);
    void StartSource(entt::registry& reg, entt::entity e, const AudioSourceComponent& s);

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
