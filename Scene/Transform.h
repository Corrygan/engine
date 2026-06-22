#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include "GameObject.h"

inline glm::mat4 BuildModelMatrix(const GameObject& obj) {
    glm::mat4 t = glm::translate(glm::mat4(1.0f),
        glm::vec3(obj.position[0], obj.position[1], obj.position[2]));
    glm::quat q(obj.rotQuat[3], obj.rotQuat[0], obj.rotQuat[1], obj.rotQuat[2]);
    glm::mat4 r = glm::mat4_cast(q);
    glm::mat4 s = glm::scale(glm::mat4(1.0f),
        glm::vec3(obj.scale[0], obj.scale[1], obj.scale[2]));
    return t * r * s;
}
