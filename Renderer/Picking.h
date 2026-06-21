#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "../Scene/GameObject.h"

// Бросает луч от камеры через точку экрана (в NDC, -1..1 по X и Y) и
// возвращает индекс ближайшего объекта, в который луч попал, либо -1.
int PickObject(const std::vector<GameObject>& objects,
    const glm::mat4& view, const glm::mat4& projection,
    float mouseNdcX, float mouseNdcY);
