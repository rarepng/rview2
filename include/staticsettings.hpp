#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>

struct staticsettings {
	glm::vec3 msworldpos{ 0.0f };
	glm::vec3 msworldrot{ 0.0f,1.0f,0.0f };
	glm::vec3 msworldscale{ 10.0f };
	bool msdrawmodel{ true };
	float rotang{ 1.57f };

};