#include "Texture.h"
#include "glad/gl.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

Texture::~Texture() {
    if (m_id) glDeleteTextures(1, &m_id);
}

Texture* Texture::LoadFromFile(const std::string& path) {
    stbi_set_flip_vertically_on_load(true);

    int w, h, ch;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 4);
    if (!data) return nullptr;

    auto* tex = new Texture();
    tex->m_width  = w;
    tex->m_height = h;

    glGenTextures(1, &tex->m_id);
    glBindTexture(GL_TEXTURE_2D, tex->m_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);
    return tex;
}

void Texture::Bind(int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, m_id);
}

void Texture::Unbind() const {
    glBindTexture(GL_TEXTURE_2D, 0);
}
