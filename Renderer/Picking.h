#pragma once
#include <glm/glm.hpp>
#include "../Scene/Scene.h"

// Returns the closest pickable entity (one with a MeshComponent) hit by the ray
// through the given NDC point, or entt::null if nothing is under the cursor.
entt::entity PickObject(const Scene& scene,
    const glm::mat4& view, const glm::mat4& projection,
    float mouseNdcX, float mouseNdcY);
