#pragma once
#include <string>

// Wrap a source audio file (wav / ogg / mp3 / flac) into a branded clip:
//   category 0 -> .fcsnd (sound),  1 -> .fcmsc (music),  2 -> .fcprs (voice)
// The encoded source bytes are embedded after a small settings header (see
// Audio/AudioClipFormat.h). Returns the written path, or "" on failure.
namespace AudioImporter {
    std::string Import(const std::string& sourcePath, int category, const std::string& destDir);
}
