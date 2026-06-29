#include "AudioEngine.h"
#include "AudioClipFormat.h"

#include <miniaudio.h>

#include <vector>
#include <memory>
#include <fstream>
#include <cstring>
#include <unordered_map>

namespace {
    // A decoded, playing one-shot. The encoded bytes + decoder must outlive the
    // sound, so all three live together.
    struct Voice {
        std::vector<unsigned char> data;
        ma_decoder decoder{};
        ma_sound   sound{};
        bool       spatial = false;
    };

    struct LoadedClip {
        std::vector<unsigned char> encoded;   // WAV / OGG / MP3 bytes
        int   bus     = 0;
        float volume  = 1.0f;
        bool  loop    = false;
        bool  spatial = true;
    };

    template <class T> void ReadPod(std::ifstream& f, T& v) {
        f.read(reinterpret_cast<char*>(&v), sizeof(T));
    }

    // Load a clip file: our FCAU container (header + embedded audio) or, for dev
    // convenience, a raw audio file (the whole file is the encoded audio).
    bool LoadClipFile(const std::string& path, LoadedClip& out) {
        std::ifstream f(path, std::ios::binary);
        if (!f.is_open()) return false;

        char magic[4] = {};
        f.read(magic, 4);
        if (std::memcmp(magic, audiofmt::kMagic, 4) == 0) {
            uint32_t version = 0, category = 0, bus = 0, dataSize = 0;
            float volume = 1.0f;
            uint8_t loop = 0, spatial = 1, stream = 0, pad = 0;
            ReadPod(f, version);
            ReadPod(f, category);
            ReadPod(f, bus);
            ReadPod(f, volume);
            ReadPod(f, loop);
            ReadPod(f, spatial);
            ReadPod(f, stream);
            ReadPod(f, pad);
            ReadPod(f, dataSize);
            if (!f || dataSize == 0) return false;
            out.encoded.resize(dataSize);
            f.read(reinterpret_cast<char*>(out.encoded.data()), dataSize);
            if (!f) return false;
            out.bus     = (int)bus;
            out.volume  = volume;
            out.loop    = loop != 0;
            out.spatial = spatial != 0;
            return true;
        }

        // Raw file: rewind and read all of it as encoded audio.
        f.clear();
        f.seekg(0, std::ios::end);
        std::streamoff sz = f.tellg();
        if (sz <= 0) return false;
        f.seekg(0, std::ios::beg);
        out.encoded.resize((size_t)sz);
        f.read(reinterpret_cast<char*>(out.encoded.data()), sz);
        return (bool)f;
    }
}

struct AudioEngine::Impl {
    ma_engine       engine{};
    ma_sound_group  buses[3]{};   // SFX, Music, Voice
    std::vector<std::unique_ptr<Voice>> voices;                       // fire-and-forget
    std::unordered_map<entt::entity, std::unique_ptr<Voice>> sources; // entity-bound
    bool ready = false;
};

AudioEngine::AudioEngine() : m_impl(std::make_unique<Impl>()) {}
AudioEngine::~AudioEngine() { Shutdown(); }

bool AudioEngine::Ready() const { return m_impl->ready; }

bool AudioEngine::Init() {
    if (m_impl->ready) return true;
    if (ma_engine_init(nullptr, &m_impl->engine) != MA_SUCCESS) return false;
    for (int i = 0; i < 3; ++i)
        ma_sound_group_init(&m_impl->engine, 0, nullptr, &m_impl->buses[i]);
    m_impl->ready = true;
    return true;
}

void AudioEngine::Shutdown() {
    if (!m_impl || !m_impl->ready) return;
    for (auto& kv : m_impl->sources) {
        ma_sound_uninit(&kv.second->sound);
        ma_decoder_uninit(&kv.second->decoder);
    }
    m_impl->sources.clear();
    for (auto& v : m_impl->voices) {
        ma_sound_uninit(&v->sound);
        ma_decoder_uninit(&v->decoder);
    }
    m_impl->voices.clear();
    for (int i = 0; i < 3; ++i)
        ma_sound_group_uninit(&m_impl->buses[i]);
    ma_engine_uninit(&m_impl->engine);
    m_impl->ready = false;
}

void AudioEngine::SetListener(const glm::vec3& pos, const glm::vec3& fwd, const glm::vec3& up) {
    if (!m_impl->ready) return;
    ma_engine_listener_set_position (&m_impl->engine, 0, pos.x, pos.y, pos.z);
    ma_engine_listener_set_direction(&m_impl->engine, 0, fwd.x, fwd.y, fwd.z);
    ma_engine_listener_set_world_up (&m_impl->engine, 0, up.x,  up.y,  up.z);
}

void AudioEngine::PlayInternal(const std::string& clip, bool spatial, const glm::vec3& pos, float volume) {
    if (!m_impl->ready) return;

    LoadedClip lc;
    if (!LoadClipFile(clip, lc)) return;

    auto v = std::make_unique<Voice>();
    v->data = std::move(lc.encoded);
    if (ma_decoder_init_memory(v->data.data(), v->data.size(), nullptr, &v->decoder) != MA_SUCCESS)
        return;

    int busIdx = (lc.bus >= 0 && lc.bus < 3) ? lc.bus : 0;
    if (ma_sound_init_from_data_source(&m_impl->engine, &v->decoder, 0,
                                       &m_impl->buses[busIdx], &v->sound) != MA_SUCCESS) {
        ma_decoder_uninit(&v->decoder);
        return;
    }

    ma_sound_set_volume(&v->sound, volume * lc.volume);
    const bool use3D = spatial && lc.spatial;
    ma_sound_set_spatialization_enabled(&v->sound, use3D ? MA_TRUE : MA_FALSE);
    if (use3D) ma_sound_set_position(&v->sound, pos.x, pos.y, pos.z);
    if (lc.loop) ma_sound_set_looping(&v->sound, MA_TRUE);

    ma_sound_start(&v->sound);
    m_impl->voices.push_back(std::move(v));
}

void AudioEngine::Play(const std::string& clip, float volume) {
    PlayInternal(clip, /*spatial*/ false, glm::vec3(0.0f), volume);
}

void AudioEngine::PlayAt(const std::string& clip, const glm::vec3& pos, float volume) {
    PlayInternal(clip, /*spatial*/ true, pos, volume);
}

void AudioEngine::Update() {
    if (!m_impl->ready) return;
    for (auto it = m_impl->voices.begin(); it != m_impl->voices.end();) {
        if (ma_sound_at_end(&(*it)->sound)) {
            ma_sound_uninit(&(*it)->sound);
            ma_decoder_uninit(&(*it)->decoder);
            it = m_impl->voices.erase(it);
        } else {
            ++it;
        }
    }
}

void AudioEngine::SetBusVolume(AudioBus bus, float volume) {
    if (!m_impl->ready) return;
    int i = (int)bus;
    if (i >= 0 && i < 3) ma_sound_group_set_volume(&m_impl->buses[i], volume);
}

float AudioEngine::BusVolume(AudioBus bus) const {
    if (!m_impl->ready) return 0.0f;
    int i = (int)bus;
    return (i >= 0 && i < 3) ? ma_sound_group_get_volume(&m_impl->buses[i]) : 0.0f;
}

void AudioEngine::StartSource(entt::registry& reg, entt::entity e, const AudioSourceComponent& s) {
    if (!m_impl->ready || s.clip.empty()) return;

    LoadedClip lc;
    if (!LoadClipFile(s.clip, lc)) return;

    auto v = std::make_unique<Voice>();
    v->data = std::move(lc.encoded);
    if (ma_decoder_init_memory(v->data.data(), v->data.size(), nullptr, &v->decoder) != MA_SUCCESS)
        return;

    int busIdx = (lc.bus >= 0 && lc.bus < 3) ? lc.bus : 0;
    if (ma_sound_init_from_data_source(&m_impl->engine, &v->decoder, 0,
                                       &m_impl->buses[busIdx], &v->sound) != MA_SUCCESS) {
        ma_decoder_uninit(&v->decoder);
        return;
    }

    v->spatial = s.spatial && lc.spatial;
    ma_sound_set_volume(&v->sound, s.volume * lc.volume);
    ma_sound_set_pitch(&v->sound, s.pitch);
    ma_sound_set_looping(&v->sound, (s.loop || lc.loop) ? MA_TRUE : MA_FALSE);
    ma_sound_set_spatialization_enabled(&v->sound, v->spatial ? MA_TRUE : MA_FALSE);
    if (v->spatial) {
        glm::vec3 p = glm::vec3(WorldMatrixOf(reg, e)[3]);
        ma_sound_set_position(&v->sound, p.x, p.y, p.z);
        ma_sound_set_min_distance(&v->sound, s.minDistance);
        ma_sound_set_max_distance(&v->sound, s.maxDistance);
    }
    ma_sound_start(&v->sound);
    m_impl->sources[e] = std::move(v);
}

void AudioEngine::BeginScene(Scene& scene) {
    EndScene();
    if (!m_impl->ready) return;
    entt::registry& reg = scene.Reg();
    UpdateWorldTransforms(reg);
    for (entt::entity e : reg.view<AudioSourceComponent, TransformComponent>()) {
        const AudioSourceComponent& s = reg.get<AudioSourceComponent>(e);
        if (s.playOnStart) StartSource(reg, e, s);
    }
}

void AudioEngine::UpdateScene(Scene& scene, const glm::vec3& camPos,
                              const glm::vec3& camForward, const glm::vec3& camUp) {
    if (!m_impl->ready) return;
    entt::registry& reg = scene.Reg();

    // Listener: an AudioListenerComponent entity if present, else the camera.
    glm::vec3 lp = camPos, lf = camForward, lu = camUp;
    for (entt::entity e : reg.view<AudioListenerComponent, TransformComponent>()) {
        glm::mat4 w = WorldMatrixOf(reg, e);
        lp = glm::vec3(w[3]);
        lf = -glm::vec3(w[2]);
        lu =  glm::vec3(w[1]);
        break;
    }
    SetListener(lp, lf, lu);

    // Sync 3D positions; free finished one-shots / orphaned sources.
    for (auto it = m_impl->sources.begin(); it != m_impl->sources.end();) {
        entt::entity e = it->first;
        Voice* v = it->second.get();
        if (!reg.valid(e) || ma_sound_at_end(&v->sound)) {
            ma_sound_uninit(&v->sound);
            ma_decoder_uninit(&v->decoder);
            it = m_impl->sources.erase(it);
            continue;
        }
        if (v->spatial) {
            glm::vec3 p = glm::vec3(WorldMatrixOf(reg, e)[3]);
            ma_sound_set_position(&v->sound, p.x, p.y, p.z);
        }
        ++it;
    }
}

void AudioEngine::EndScene() {
    if (!m_impl) return;
    for (auto& kv : m_impl->sources) {
        ma_sound_uninit(&kv.second->sound);
        ma_decoder_uninit(&kv.second->decoder);
    }
    m_impl->sources.clear();
}

void AudioEngine::PlaySource(Scene& scene, entt::entity e) {
    if (!m_impl->ready) return;
    entt::registry& reg = scene.Reg();
    if (!reg.valid(e)) return;
    const AudioSourceComponent* s = reg.try_get<AudioSourceComponent>(e);
    if (!s) return;

    auto it = m_impl->sources.find(e);
    if (it != m_impl->sources.end()) {
        ma_sound_uninit(&it->second->sound);
        ma_decoder_uninit(&it->second->decoder);
        m_impl->sources.erase(it);
    }
    StartSource(reg, e, *s);
}
