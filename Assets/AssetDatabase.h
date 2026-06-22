#pragma once
#include <string>
#include "../Core/Guid.h"

class AssetDatabase {
public:
    static Guid GetOrCreateGuid(const std::string& assetPath, const std::string& assetType);
    static int ScanFolder(const std::string& folderPath);

private:
    static std::string MetaPathFor(const std::string& assetPath);
};
