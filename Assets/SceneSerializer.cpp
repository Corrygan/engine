#include "SceneSerializer.h"
#include "../Core/Guid.h"
#include <cstdint>
#include <cstring>
#include <fstream>

namespace {
    constexpr char kMagic[4] = { 'E', 'S', 'C', 'N' };
    constexpr uint32_t kVersion = 1;

    void WriteString(std::ofstream& file, const std::string& str) {
        uint32_t len = static_cast<uint32_t>(str.size());
        file.write(reinterpret_cast<const char*>(&len), sizeof(len));
        file.write(str.data(), len);
    }

    bool ReadString(std::ifstream& file, std::string& out) {
        uint32_t len = 0;
        if (!file.read(reinterpret_cast<char*>(&len), sizeof(len))) return false;
        out.resize(len);
        if (len > 0 && !file.read(out.data(), len)) return false;
        return true;
    }
}

bool SceneSerializer::Save(const std::string& path, const std::vector<GameObject>& objects) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    file.write(kMagic, 4);
    file.write(reinterpret_cast<const char*>(&kVersion), sizeof(kVersion));

    // У сцены тоже свой GUID — на случай, если позже захотим ссылаться
    // на сцены друг из друга (например, для аддитивной загрузки).
    Guid sceneGuid = Guid::Generate();
    file.write(reinterpret_cast<const char*>(sceneGuid.bytes.data()), sceneGuid.bytes.size());

    uint32_t count = static_cast<uint32_t>(objects.size());
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (const auto& obj : objects) {
        WriteString(file, obj.name);
        file.write(reinterpret_cast<const char*>(obj.position), sizeof(obj.position));
        file.write(reinterpret_cast<const char*>(obj.rotation), sizeof(obj.rotation));
        file.write(reinterpret_cast<const char*>(obj.scale), sizeof(obj.scale));
    }

    return true;
}

bool SceneSerializer::Load(const std::string& path, std::vector<GameObject>& outObjects) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    char magic[4];
    if (!file.read(magic, 4)) return false;
    if (std::memcmp(magic, kMagic, 4) != 0) return false;  // не наш формат файла

    uint32_t version = 0;
    if (!file.read(reinterpret_cast<char*>(&version), sizeof(version))) return false;
    if (version != kVersion) return false;  // сюда позже добавим миграцию между версиями

    Guid sceneGuid;
    if (!file.read(reinterpret_cast<char*>(sceneGuid.bytes.data()), sceneGuid.bytes.size())) return false;

    uint32_t count = 0;
    if (!file.read(reinterpret_cast<char*>(&count), sizeof(count))) return false;

    outObjects.clear();
    outObjects.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        GameObject obj;
        if (!ReadString(file, obj.name)) return false;
        if (!file.read(reinterpret_cast<char*>(obj.position), sizeof(obj.position))) return false;
        if (!file.read(reinterpret_cast<char*>(obj.rotation), sizeof(obj.rotation))) return false;
        if (!file.read(reinterpret_cast<char*>(obj.scale), sizeof(obj.scale))) return false;
        outObjects.push_back(std::move(obj));
    }

    return true;
}
