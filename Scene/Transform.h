#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "GameObject.h"

// Общая функция построения матрицы модели из Transform-полей GameObject'а.
// Используется и рендерером, и picking'ом — важно, чтобы они никогда не
// расходились, иначе клик "не попадёт" туда, где объект визуально нарисован.
inline glm::mat4 BuildModelMatrix(const GameObject& obj) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f),
        glm::vec3(obj.position[0], obj.position[1], obj.position[2]));
    model = glm::rotate(model, glm::radians(obj.rotation[0]), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(obj.rotation[1]), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(obj.rotation[2]), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, glm::vec3(obj.scale[0], obj.scale[1], obj.scale[2]));
    return model;
}
