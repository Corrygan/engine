#pragma once
#include <cstdint>
#include <string>
#include <glm/glm.hpp>

class Shader {
public:
    Shader(const std::string& vertexSrc, const std::string& fragmentSrc);
    ~Shader();

    void Bind() const;
    void Unbind() const;

    void SetMat4(const std::string& name, const glm::mat4& matrix) const;
    void SetVec2(const std::string& name, const glm::vec2& v) const;
    void SetVec3(const std::string& name, const glm::vec3& v) const;
    void SetFloat(const std::string& name, float value) const;
    void SetInt  (const std::string& name, int   value) const;
    uint32_t GetID() const { return m_programID; }

private:
    uint32_t Compile(uint32_t type, const std::string& source);

    uint32_t m_programID = 0;
};
