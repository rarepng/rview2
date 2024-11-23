
#define GLM_ENABLE_EXPERIMENTAL
#include "arrow.hpp"


vkmesh arrow::getmesh() {
	if (mmesh.verts.size() == 0)init();
	return mmesh;
}
void arrow::init() {
	mmesh.verts.reserve(6);
	mmesh.verts.resize(6);

	mmesh.verts[0].pos = glm::vec3{ 0.0f, 0.0f, 0.0f };
	mmesh.verts[1].pos = glm::vec3{ 1.0f, 0.0f, 0.0f };
	mmesh.verts[2].pos = glm::vec3{ 1.0f, 0.0f, 0.0f };
	mmesh.verts[3].pos = glm::vec3{ 0.8f, 0.0f, 0.075f };
	mmesh.verts[4].pos = glm::vec3{ 1.0f, 0.0f, 0.0f };
	mmesh.verts[5].pos = glm::vec3{ 0.8f, 0.0f, -0.075f };


	mmesh.verts[0].col = glm::vec3{ 0.8f,0.0f,1.0f };
	mmesh.verts[1].col = glm::vec3{ 0.8f,0.0f,0.0f };
	mmesh.verts[2].col = glm::vec3{ 0.8f,0.0f,0.0f };
	mmesh.verts[3].col = glm::vec3{ 0.8f,0.0f,0.0f };
	mmesh.verts[4].col = glm::vec3{ 0.8f,0.0f,0.0f };
	mmesh.verts[5].col = glm::vec3{ 0.8f,0.0f,0.0f };


	//mmesh.verts[0].uv = glm::vec2{ 0.0, 1.0 };
	//mmesh.verts[1].uv = glm::vec2{ 1.0, 0.0 };
	//mmesh.verts[2].uv = glm::vec2{ 0.0, 0.0 };
	//mmesh.verts[3].uv = glm::vec2{ 0.0, 1.0 };
	//mmesh.verts[4].uv = glm::vec2{ 1.0, 1.0 };
	//mmesh.verts[5].uv = glm::vec2{ 1.0, 0.0 };




}

