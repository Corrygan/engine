#pragma once
#include <map>
#include <string>
#include "../Core/Guid.h"

// Мета-файл-спутник для импортированных ассетов (model.fbx -> model.fbx.meta).
// Текстовый формат — читаемый глазами, легко смотреть diff в git.
class MetaFile {
public:
    Guid guid;
    std::string type;  // например "Texture", "Model", "Scene"
    std::map<std::string, std::string> extra;  // место для будущих настроек импорта

    bool Save(const std::string& metaPath) const;
    static bool Load(const std::string& metaPath, MetaFile& out);
};
