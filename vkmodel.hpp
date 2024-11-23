#pragma once
#include <vector>
#include <glm/glm.hpp>

#include "vkobjs.hpp"

class vkmodel {
public:
	vkmesh getmesh();
private:
	void init();
	vkmesh mmesh;
};