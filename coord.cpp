
#define GLM_ENABLE_EXPERIMENTAL
#include "coord.hpp"


vkmesh coord::getmesh() {
	if (mmesh.verts.size() == 0)init();
	return mmesh;
}
void coord::init() {
	mmesh.verts.reserve(18);
	mmesh.verts.resize(18);

	mmesh.verts[0].pos = glm::vec3{ 0.0f, 0.0f, 0.0f };
	mmesh.verts[1].pos = glm::vec3{ 1.0f, 0.0f, 0.0f };
	mmesh.verts[2].pos = glm::vec3{ 1.0f, 0.0f, 0.0f };
	mmesh.verts[3].pos = glm::vec3{ 0.8f, 0.0f, 0.075f };
	mmesh.verts[4].pos = glm::vec3{ 1.0f, 0.0f, 0.0f };
	mmesh.verts[5].pos = glm::vec3{ 0.8f, 0.0f, -0.075f };


	mmesh.verts[0].col = glm::vec3{ 0.8f,0.0f,0.0f };
	mmesh.verts[1].col = glm::vec3{ 0.8f,0.0f,0.0f };
	mmesh.verts[2].col = glm::vec3{ 0.8f,0.0f,0.0f };
	mmesh.verts[3].col = glm::vec3{ 0.8f,0.0f,0.0f };
	mmesh.verts[4].col = glm::vec3{ 0.8f,0.0f,0.0f };
	mmesh.verts[5].col = glm::vec3{ 0.8f,0.0f,0.0f };



	mmesh.verts[6].pos = glm::vec3{ 0.0f, 0.0f, 0.0f };
	mmesh.verts[7].pos = glm::vec3{ 0.0f, 1.0f, 0.0f };
	mmesh.verts[8].pos = glm::vec3{ 0.0f, 1.0f, 0.0f };
	mmesh.verts[9].pos = glm::vec3{ 0.0f, 0.8f, 0.075f };
	mmesh.verts[10].pos = glm::vec3{ 0.0f, 1.0f, 0.0f };
	mmesh.verts[11].pos = glm::vec3{ 0.0f, 0.8f, -0.075f };


	mmesh.verts[6].col = glm::vec3{ 0.0f,0.0f,0.8f };
	mmesh.verts[7].col = glm::vec3{ 0.0f,0.0f,0.8f };
	mmesh.verts[8].col = glm::vec3{ 0.0f,0.0f,0.8f };
	mmesh.verts[9].col = glm::vec3{ 0.0f,0.0f,0.8f };
	mmesh.verts[10].col = glm::vec3{ 0.0f,0.0f,0.8f };
	mmesh.verts[11].col = glm::vec3{ 0.0f,0.0f,0.8f };


	mmesh.verts[12].pos = glm::vec3{ 0.0f, 0.0f, 0.0f };
	mmesh.verts[13].pos = glm::vec3{ 0.0f, 0.0f, 1.0f };
	mmesh.verts[14].pos = glm::vec3{ 0.0f, 0.0f, 1.0f };
	mmesh.verts[15].pos = glm::vec3{ 0.075f, 0.0f,0.8f };
	mmesh.verts[16].pos = glm::vec3{ 0.0f, 0.0f, 1.0f };
	mmesh.verts[17].pos = glm::vec3{ -0.075f, 0.0f,0.8f };


	mmesh.verts[12].col = glm::vec3{ 0.0f,0.8f,0.0f };
	mmesh.verts[13].col = glm::vec3{ 0.0f,0.8f,0.0f };
	mmesh.verts[14].col = glm::vec3{ 0.0f,0.8f,0.0f };
	mmesh.verts[15].col = glm::vec3{ 0.0f,0.8f,0.0f };
	mmesh.verts[16].col = glm::vec3{ 0.0f,0.8f,0.0f };
	mmesh.verts[17].col = glm::vec3{ 0.0f,0.8f,0.0f };

	//mmesh.verts[0].uv = glm::vec2{ 0.0, 1.0 };
	//mmesh.verts[1].uv = glm::vec2{ 1.0, 0.0 };
	//mmesh.verts[2].uv = glm::vec2{ 0.0, 0.0 };
	//mmesh.verts[3].uv = glm::vec2{ 0.0, 1.0 };
	//mmesh.verts[4].uv = glm::vec2{ 1.0, 1.0 };
	//mmesh.verts[5].uv = glm::vec2{ 1.0, 0.0 };

}

