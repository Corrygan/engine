#pragma once
#include <cstdint>

// Branded audio container used by .fcsnd (sound), .fcmsc (music) and .fcprs
// (voice/phrase). A small header carries import settings, followed by the
// original ENCODED audio bytes (WAV / OGG / MP3) — miniaudio detects the codec
// from the embedded data, so the wrapper extension is free.
//
// On-disk layout (little-endian), written/read field by field:
//   char   magic[4]  = "FCAU"
//   u32    version
//   u32    category  (0 sound, 1 music, 2 voice)
//   u32    bus       (0 SFX, 1 Music, 2 Voice)
//   f32    volume
//   u8     loop
//   u8     spatial   (1 = 3D positioned)
//   u8     stream    (reserved; v1 decodes fully into memory)
//   u8     _pad
//   u32    dataSize
//   u8     data[dataSize]   // encoded audio
namespace audiofmt {
    constexpr char     kMagic[4] = { 'F', 'C', 'A', 'U' };
    constexpr uint32_t kVersion  = 1;

    enum class Category : uint32_t { Sound = 0, Music = 1, Voice = 2 };
    enum class Bus      : uint32_t { SFX = 0, Music = 1, Voice = 2 };
}
