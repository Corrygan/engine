#include "AssetDatabase.h"
#include "MetaFile.h"
#include "ModelImporter.h"
#include <filesystem>

namespace fs = std::filesystem;

static bool IsModelSource(const std::string& ext) {
    return ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb";
}

std::string AssetDatabase::MetaPathFor(const std::string& assetPath) {
    return assetPath + ".meta";
}

Guid AssetDatabase::GetOrCreateGuid(const std::string& assetPath, const std::string& assetType) {
    const std::string metaPath = MetaPathFor(assetPath);

    MetaFile meta;
    if (MetaFile::Load(metaPath, meta) && meta.guid.IsValid()) {
        return meta.guid;
    }

    meta.guid = Guid::Generate();
    meta.type = assetType;
    meta.Save(metaPath);
    return meta.guid;
}

int AssetDatabase::ScanFolder(const std::string& folderPath) {
    if (!fs::exists(folderPath)) return 0;

    int createdCount = 0;
    for (const auto& entry : fs::recursive_directory_iterator(folderPath)) {
        if (!entry.is_regular_file()) continue;

        const std::string path = entry.path().string();
        const std::string ext  = entry.path().extension().string();

        // .graph is a sub-asset of its .emat — no standalone GUID needed.
        if (ext == ".meta" || ext == ".emdl" || ext == ".fcscn" || ext == ".escn" || ext == ".graph") continue;

        if (IsModelSource(ext)) {
            if (ModelImporter::NeedsReimport(path)) {
                ModelImporter::Import(path);
                ++createdCount;
            }
            continue;
        }

        if (!fs::exists(MetaPathFor(path))) {
            GetOrCreateGuid(path, ext.empty() ? "Unknown" : ext.substr(1));
            ++createdCount;
        }
    }
    return createdCount;
}
