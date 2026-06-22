#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

struct Guid {
    std::array<uint8_t, 16> bytes{};

    bool IsValid() const;
    std::string ToString() const;

    static Guid FromString(const std::string& str);
    static Guid Generate();

    bool operator==(const Guid& other) const { return bytes == other.bytes; }
    bool operator!=(const Guid& other) const { return !(*this == other); }
};

struct GuidHash {
    size_t operator()(const Guid& g) const;
};
