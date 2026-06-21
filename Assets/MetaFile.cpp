#include "MetaFile.h"
#include <fstream>

bool MetaFile::Save(const std::string& metaPath) const {
    std::ofstream file(metaPath);
    if (!file.is_open()) return false;

    file << "guid=" << guid.ToString() << "\n";
    file << "type=" << type << "\n";
    for (const auto& [key, value] : extra) {
        file << key << "=" << value << "\n";
    }
    return true;
}

bool MetaFile::Load(const std::string& metaPath, MetaFile& out) {
    std::ifstream file(metaPath);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        if (key == "guid") out.guid = Guid::FromString(value);
        else if (key == "type") out.type = value;
        else out.extra[key] = value;
    }
    return true;
}
