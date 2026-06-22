#pragma once
#include <cstdint>
#include <vector>

class PrimitiveMesh {
public:
    virtual ~PrimitiveMesh();

    void Draw() const;

protected:
    PrimitiveMesh() = default;

    void Upload(const std::vector<float>& vertices);

private:
    uint32_t m_vao = 0;
    uint32_t m_vbo = 0;
    int m_vertexCount = 0;
};

class CubeMesh : public PrimitiveMesh {
public:
    CubeMesh();
};

class SphereMesh : public PrimitiveMesh {
public:
    explicit SphereMesh(int latSegments = 16, int lonSegments = 24);
};

class PlaneMesh : public PrimitiveMesh {
public:
    PlaneMesh();
};