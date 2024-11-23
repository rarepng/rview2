#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <vector>
#include <glm/glm.hpp>
#include "vkobjs.hpp"
class arrow {
public:
	vkmesh getmesh();
private:
	void init();
	vkmesh mmesh;
};