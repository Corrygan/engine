#include "Shader.h"
#include "glad/gl.h"
#include <iostream>

Shader::Shader(const std::string& vertexSrc, const std::string& fragmentSrc) {
    uint32_t vs = Compile(GL_VERTEX_SHADER, vertexSrc);
    uint32_t fs = Compile(GL_FRAGMENT_SHADER, fragmentSrc);

    m_programID = glCreateProgram();
    glAttachShader(m_programID, vs);
    glAttachShader(m_programID, fs);
    glLinkProgram(m_programID);

    int success = 0;
    glGetProgramiv(m_programID, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(m_programID, sizeof(log), nullptr, log);
        std::cerr << "Shader link error: " << log << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
}

Shader::~Shader() {
    if (m_programID) glDeleteProgram(m_programID);
}

uint32_t Shader::Compile(uint32_t type, const std::string& source) {
    uint32_t shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "Shader compile error (" << (type == GL_VERTEX_SHADER ? "vertex" : "fragment") << "): " << log << std::endl;
    }
    return shader;
}

void Shader::Bind() const {
    glUseProgram(m_programID);
}

void Shader::Unbind() const {
    glUseProgram(0);
}

void Shader::SetMat4(const std::string& name, const glm::mat4& matrix) const {
    int loc = glGetUniformLocation(m_programID, name.c_str());
    glUniformMatrix4fv(loc, 1, GL_FALSE, &matrix[0][0]);
}

void Shader::SetVec2(const std::string& name, const glm::vec2& v) const {
    int loc = glGetUniformLocation(m_programID, name.c_str());
    glUniform2f(loc, v.x, v.y);
}

void Shader::SetVec3(const std::string& name, const glm::vec3& v) const {
    int loc = glGetUniformLocation(m_programID, name.c_str());
    glUniform3f(loc, v.x, v.y, v.z);
}

void Shader::SetFloat(const std::string& name, float value) const {
    int loc = glGetUniformLocation(m_programID, name.c_str());
    glUniform1f(loc, value);
}

void Shader::SetInt(const std::string& name, int value) const {
    int loc = glGetUniformLocation(m_programID, name.c_str());
    glUniform1i(loc, value);
}
