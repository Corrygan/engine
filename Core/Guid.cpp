#include "Guid.h"
#include <iomanip>
#include <random>
#include <sstream>

bool Guid::IsValid() const {
    for (uint8_t b : bytes) {
        if (b != 0) return true;
    }
    return false;
}

std::string Guid::ToString() const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < bytes.size(); ++i) {
        oss << std::setw(2) << static_cast<int>(bytes[i]);
        if (i == 3 || i == 5 || i == 7 || i == 9) oss << '-';
    }
    return oss.str();
}

Guid Guid::FromString(const std::string& str) {
    Guid guid;
    size_t byteIndex = 0;
    for (size_t i = 0; i < str.size() && byteIndex < guid.bytes.size(); ) {
        if (str[i] == '-') {
            ++i;
            continue;
        }
        if (i + 1 >= str.size()) break;
        guid.bytes[byteIndex++] = static_cast<uint8_t>(std::stoul(str.substr(i, 2), nullptr, 16));
        i += 2;
    }
    return guid;
}

Guid Guid::Generate() {
    // thread_local so Generate() is safe to call from job-system workers
    // (e.g. background asset import) concurrently with the main thread.
    thread_local std::mt19937_64 engine(std::random_device{}());
    thread_local std::uniform_int_distribution<uint32_t> dist(0, 255);

    Guid guid;
    for (auto& b : guid.bytes) {
        b = static_cast<uint8_t>(dist(engine));
    }

    guid.bytes[6] = (guid.bytes[6] & 0x0F) | 0x40;
    guid.bytes[8] = (guid.bytes[8] & 0x3F) | 0x80;

    return guid;
}

size_t GuidHash::operator()(const Guid& g) const {
    size_t h = 0;
    for (uint8_t b : g.bytes) {
        h = h * 31 + b;
    }
    return h;
}
