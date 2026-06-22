#pragma once
#include <string>

class ModelImporter {
public:
    static std::string Import(const std::string& sourcePath);
    static bool NeedsReimport(const std::string& sourcePath);

private:
    static std::string EmdlPathFor(const std::string& sourcePath);
};
