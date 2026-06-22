#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "../Scene/GameObject.h"

int PickObject(const std::vector<GameObject>& objects,
    const glm::mat4& view, const glm::mat4& projection,
    float mouseNdcX, float mouseNdcY);
