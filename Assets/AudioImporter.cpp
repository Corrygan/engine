#include "AudioImporter.h"
#include "../Audio/AudioClipFormat.h"

#include <fstream>
#include <filesystem>
#include <vector>
#include <iterator>
#include <cstdint>

namespace fs = std::filesystem;

namespace {
    template <class T> void Write(std::ofstream& f, const T& v) {
        f.write(reinterpret_cast<const char*>(&v), sizeof(T));
    }
}

namespace AudioImporter {

std::string Import(const std::string& sourcePath, int category, const std::string& destDir) {
    std::ifstream in(sourcePath, std::ios::binary);
    if (!in.is_open()) return {};
    std::vector<char> bytes((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
    if (bytes.empty()) return {};

    // Category subfolder under Assets (created on demand), like Materials/Scripts.
    const char* ext = (category == 1) ? ".fcmsc" : (category == 2) ? ".fcprs" : ".fcsnd";
    const char* sub = (category == 1) ? "Music"  : (category == 2) ? "Voice"  : "Sounds";
    std::error_code ec;
    fs::path dir = fs::path(destDir) / sub;
    fs::create_directories(dir, ec);
    std::string stem = fs::path(sourcePath).stem().string();
    std::string out  = (dir / (stem + ext)).string();

    std::ofstream f(out, std::ios::binary);
    if (!f.is_open()) return {};

    f.write(audiofmt::kMagic, 4);
    uint32_t version  = audiofmt::kVersion;
    uint32_t cat      = (uint32_t)category;
    uint32_t bus      = (uint32_t)category;               // sound->SFX, music->Music, voice->Voice
    float    volume   = 1.0f;
    uint8_t  loop     = (category == 1) ? 1 : 0;           // music loops by default
    uint8_t  spatial  = (category == 0) ? 1 : 0;           // only sounds are 3D by default
    uint8_t  stream   = (category == 1) ? 1 : 0;           // reserved (v1 decodes in memory)
    uint8_t  pad      = 0;
    uint32_t dataSize = (uint32_t)bytes.size();
    Write(f, version); Write(f, cat); Write(f, bus); Write(f, volume);
    Write(f, loop); Write(f, spatial); Write(f, stream); Write(f, pad);
    Write(f, dataSize);
    f.write(bytes.data(), (std::streamsize)bytes.size());
    return f.good() ? out : std::string();
}

} // namespace AudioImporter
