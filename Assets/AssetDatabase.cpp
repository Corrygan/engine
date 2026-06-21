#include "AssetDatabase.h"
#include "MetaFile.h"
#include <filesystem>

namespace fs = std::filesystem;

std::string AssetDatabase::MetaPathFor(const std::string& assetPath) {
    return assetPath + ".meta";
}

Guid AssetDatabase::GetOrCreateGuid(const std::string& assetPath, const std::string& assetType) {
    const std::string metaPath = MetaPathFor(assetPath);

    MetaFile meta;
    if (MetaFile::Load(metaPath, meta) && meta.guid.IsValid()) {
        return meta.guid;
    }

    // .meta нет, либо он битый/пустой — создаём новый.
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
        if (entry.path().extension() == ".meta") continue;  // сами .meta пропускаем

        if (!fs::exists(MetaPathFor(path))) {
            std::string ext = entry.path().extension().string();
            GetOrCreateGuid(path, ext.empty() ? "Unknown" : ext.substr(1));
            ++createdCount;
        }
    }
    return createdCount;
}
