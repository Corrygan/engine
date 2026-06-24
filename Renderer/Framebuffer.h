#pragma once
#include <cstdint>

class Framebuffer {
public:
    Framebuffer(int width, int height, bool hdr = false);
    ~Framebuffer();

    void Bind();
    void Unbind();

    void Resize(int width, int height);

    uint32_t GetColorTexture() const { return m_colorTexture; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

private:
    void Create();
    void Destroy();

    uint32_t m_fbo = 0;
    uint32_t m_colorTexture = 0;
    uint32_t m_depthTexture = 0;
    int m_width = 0;
    int m_height = 0;
    bool m_hdr = false;
};
