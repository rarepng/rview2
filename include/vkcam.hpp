#pragma once
#define GLM_ENABLE_EXPERIMENTAL

#include "vkobjs.hpp"
#include <glm/glm.hpp>

class vkcam {
public:
	glm::mat4 getview(rvk &mvkobjs);
	glm::vec3 mforward{0.0f};
	glm::vec3 mright{0.0f};
	glm::vec3 mup{0.0f};
	glm::vec3 wup{0.0f, 1.0f, 0.0f};
};