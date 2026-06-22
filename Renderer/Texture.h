#pragma once
#include <cstdint>
#include <string>

class Texture {
public:
    ~Texture();

    static Texture* LoadFromFile(const std::string& path);

    void     Bind(int unit = 0) const;
    void     Unbind()           const;
    uint32_t GetID()     const { return m_id;     }
    int      GetWidth()  const { return m_width;  }
    int      GetHeight() const { return m_height; }

private:
    Texture() = default;
    uint32_t m_id     = 0;
    int      m_width  = 0;
    int      m_height = 0;
};
