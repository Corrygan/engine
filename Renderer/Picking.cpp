#include "Picking.h"
#include "../Scene/Transform.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
    bool RayIntersectsUnitCube(const glm::vec3& rayOriginLocal, const glm::vec3& rayDirLocal, float& outT) {
        float tMin = -std::numeric_limits<float>::infinity();
        float tMax = std::numeric_limits<float>::infinity();

        for (int axis = 0; axis < 3; ++axis) {
            float origin = rayOriginLocal[axis];
            float dir = rayDirLocal[axis];

            if (std::abs(dir) < 1e-8f) {
                if (origin < -0.5f || origin > 0.5f) return false;
                continue;
            }

            float t1 = (-0.5f - origin) / dir;
            float t2 = (0.5f - origin) / dir;
            if (t1 > t2) std::swap(t1, t2);

            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            if (tMin > tMax) return false;
        }

        if (tMax < 0.0f) return false;
        outT = (tMin >= 0.0f) ? tMin : tMax;
        return true;
    }

    bool RayIntersectsUnitSphere(const glm::vec3& rayOriginLocal, const glm::vec3& rayDirLocal, float& outT) {
        constexpr float kRadius = 0.5f;

        float a = glm::dot(rayDirLocal, rayDirLocal);
        if (a < 1e-12f) return false;

        float b = 2.0f * glm::dot(rayOriginLocal, rayDirLocal);
        float c = glm::dot(rayOriginLocal, rayOriginLocal) - kRadius * kRadius;

        float discriminant = b * b - 4.0f * a * c;
        if (discriminant < 0.0f) return false;

        float sqrtDisc = std::sqrt(discriminant);
        float t1 = (-b - sqrtDisc) / (2.0f * a);
        float t2 = (-b + sqrtDisc) / (2.0f * a);
        if (t1 > t2) std::swap(t1, t2);

        if (t2 < 0.0f) return false;
        outT = (t1 >= 0.0f) ? t1 : t2;
        return true;
    }

    bool RayIntersectsAabb(const glm::vec3& ro, const glm::vec3& rd,
                            const float* aabbMin, const float* aabbMax, float& outT) {
        float tMin = -std::numeric_limits<float>::infinity();
        float tMax =  std::numeric_limits<float>::infinity();
        for (int i = 0; i < 3; ++i) {
            float dir = rd[i], origin = ro[i];
            if (std::abs(dir) < 1e-8f) {
                if (origin < aabbMin[i] || origin > aabbMax[i]) return false;
                continue;
            }
            float t1 = (aabbMin[i] - origin) / dir;
            float t2 = (aabbMax[i] - origin) / dir;
            if (t1 > t2) std::swap(t1, t2);
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            if (tMin > tMax) return false;
        }
        if (tMax < 0.0f) return false;
        outT = (tMin >= 0.0f) ? tMin : tMax;
        return true;
    }

    bool RayIntersectsUnitPlane(const glm::vec3& rayOriginLocal, const glm::vec3& rayDirLocal, float& outT) {
        if (std::abs(rayDirLocal.y) < 1e-8f) return false;

        float t = -rayOriginLocal.y / rayDirLocal.y;
        if (t < 0.0f) return false;

        glm::vec3 hit = rayOriginLocal + rayDirLocal * t;
        if (hit.x < -0.5f || hit.x > 0.5f || hit.z < -0.5f || hit.z > 0.5f) return false;

        outT = t;
        return true;
    }
}

int PickObject(const std::vector<GameObject>& objects,
    const glm::mat4& view, const glm::mat4& projection,
    float mouseNdcX, float mouseNdcY) {

    glm::mat4 invViewProj = glm::inverse(projection * view);

    glm::vec4 nearPoint = invViewProj * glm::vec4(mouseNdcX, mouseNdcY, -1.0f, 1.0f);
    glm::vec4 farPoint = invViewProj * glm::vec4(mouseNdcX, mouseNdcY, 1.0f, 1.0f);
    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;

    glm::vec3 rayOrigin = glm::vec3(nearPoint);
    glm::vec3 rayDir = glm::vec3(farPoint) - glm::vec3(nearPoint);

    int closestIndex = -1;
    float closestT = std::numeric_limits<float>::infinity();

    for (size_t i = 0; i < objects.size(); ++i) {
        const PrimitiveType type = objects[i].type;
        if (type == PrimitiveType::Camera || type == PrimitiveType::Light || type == PrimitiveType::Empty) {
            continue;
        }

        glm::mat4 model = BuildModelMatrix(objects[i]);
        glm::mat4 invModel = glm::inverse(model);

        glm::vec3 localOrigin = glm::vec3(invModel * glm::vec4(rayOrigin, 1.0f));
        glm::vec3 localDir = glm::vec3(invModel * glm::vec4(rayDir, 0.0f));

        float t;
        bool hit;
        switch (type) {
        case PrimitiveType::Sphere:
            hit = RayIntersectsUnitSphere(localOrigin, localDir, t);
            break;
        case PrimitiveType::Plane:
            hit = RayIntersectsUnitPlane(localOrigin, localDir, t);
            break;
        case PrimitiveType::Model:
            hit = RayIntersectsAabb(localOrigin, localDir,
                objects[i].aabbMin, objects[i].aabbMax, t);
            break;
        default:
            hit = RayIntersectsUnitCube(localOrigin, localDir, t);
            break;
        }

        if (hit && t < closestT) {
            closestT = t;
            closestIndex = static_cast<int>(i);
        }
    }

    return closestIndex;
}