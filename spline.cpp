
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/spline.hpp>
#include "spline.hpp"


vkmesh spline::createmesh(int num, glm::vec3 startv, glm::vec3 startt, glm::vec3 endv, glm::vec3 endt)
{
	vkmesh mmesh;
	mmesh.verts.resize(num * 2 + 4);
	mmesh.verts[0].col = glm::vec3{ 0.0f,0.0f,0.0f };
	mmesh.verts[0].pos = startv;
	mmesh.verts[1].col = glm::vec3{ 0.0f,0.0f,0.0f };
	mmesh.verts[1].pos = startv+startt;
	mmesh.verts[2].col = glm::vec3{ 0.8f,0.8f,0.8f };
	mmesh.verts[2].pos = endv;
	mmesh.verts[3].col = glm::vec3{ 0.8f,0.8f,0.8f };
	mmesh.verts[3].pos = endv+endt;

	float ofx = 1.0f / static_cast<float>(num);
	float v = 0.0f;

	for (int i{ 5 }; i < num * 2 + 4; i += 2) {
		mmesh.verts[i - 1].pos = glm::hermite(startv, startt, endv, endt, v);
		mmesh.verts[i - 1].col = glm::vec3{ v };
		mmesh.verts[i].col = glm::vec3{ v };
		v += ofx;
		mmesh.verts[i].pos = glm::hermite(startv, startt, endv, endt, v);
	}

	mmesh.verts[num * 2 + 4 - 1].pos = endv;
	mmesh.verts[num * 2 + 4 - 1].col = glm::vec3{ v };

	return mmesh;
}
