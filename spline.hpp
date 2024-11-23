#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <vector>
#include <glm/glm.hpp>
#include "vkobjs.hpp"
class spline {
public:
	vkmesh createmesh(int num,glm::vec3 startv,glm::vec3 startt,glm::vec3 endv,glm::vec3 endt);
};