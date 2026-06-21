#include "Picking.h"
#include "../Scene/Transform.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
    // Луч уже переведён в локальное пространство объекта (единичный куб от
    // -0.5 до 0.5 по каждой оси). rayDirLocal НЕ нормализован специально —
    // это позволяет напрямую сравнивать t между разными объектами, т.к. все
    // они проверяются относительно одного и того же мирового направления луча.
    bool RayIntersectsUnitCube(const glm::vec3& rayOriginLocal, const glm::vec3& rayDirLocal, float& outT) {
        float tMin = -std::numeric_limits<float>::infinity();
        float tMax = std::numeric_limits<float>::infinity();

        for (int axis = 0; axis < 3; ++axis) {
            float origin = rayOriginLocal[axis];
            float dir = rayDirLocal[axis];

            if (std::abs(dir) < 1e-8f) {
                // Луч параллелен этой грани — либо мы в полосе, либо мимо.
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

        if (tMax < 0.0f) return false;  // куб целиком позади начала луча
        outT = (tMin >= 0.0f) ? tMin : tMax;
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
    glm::vec3 rayDir = glm::vec3(farPoint) - glm::vec3(nearPoint);  // не нормализуем

    int closestIndex = -1;
    float closestT = std::numeric_limits<float>::infinity();

    for (size_t i = 0; i < objects.size(); ++i) {
        glm::mat4 model = BuildModelMatrix(objects[i]);
        glm::mat4 invModel = glm::inverse(model);

        glm::vec3 localOrigin = glm::vec3(invModel * glm::vec4(rayOrigin, 1.0f));
        glm::vec3 localDir = glm::vec3(invModel * glm::vec4(rayDir, 0.0f));

        float t;
        if (RayIntersectsUnitCube(localOrigin, localDir, t) && t < closestT) {
            closestT = t;
            closestIndex = static_cast<int>(i);
        }
    }

    return closestIndex;
}
