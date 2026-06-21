#pragma once
#include <cstdint>

// Рендерит сцену не напрямую в окно, а в текстуру (color attachment),
// которую потом можно показать где угодно — например, внутри ImGui::Image()
// в панели Viewport.
class Framebuffer {
public:
    Framebuffer(int width, int height);
    ~Framebuffer();

    void Bind();
    void Unbind();

    // Пересоздаёт текстуры под новый размер. Если размер не изменился — ничего не делает.
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
};
