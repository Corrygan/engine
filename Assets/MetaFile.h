#pragma once
#include <map>
#include <string>
#include "../Core/Guid.h"

class MetaFile {
public:
    Guid guid;
    std::string type;
    std::map<std::string, std::string> extra;

    bool Save(const std::string& metaPath) const;
    static bool Load(const std::string& metaPath, MetaFile& out);
};
