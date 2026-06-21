#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

// Стабильный идентификатор ассета. Все ссылки между ассетами (сцена -> меш,
// материал -> текстура и т.д.) должны идти через Guid, а не через путь к файлу —
// тогда переименование/перемещение файла в проекте не ломает ссылки на него.
struct Guid {
    std::array<uint8_t, 16> bytes{};

    bool IsValid() const;
    std::string ToString() const;

    static Guid FromString(const std::string& str);
    static Guid Generate();

    bool operator==(const Guid& other) const { return bytes == other.bytes; }
    bool operator!=(const Guid& other) const { return !(*this == other); }
};

// Чтобы Guid можно было использовать как ключ в std::unordered_map.
struct GuidHash {
    size_t operator()(const Guid& g) const;
};
