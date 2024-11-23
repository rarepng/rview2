#include "vkmodel.hpp"


vkmesh vkmodel::getmesh() {
	if (mmesh.verts.size() == 0)init();
	return mmesh;
}
void vkmodel::init() {
	mmesh.verts.reserve(36);
	mmesh.verts.resize(36);

	mmesh.verts[0].pos = glm::vec3{ -0.5f, -0.5f, 0.5f };
	mmesh.verts[1].pos = glm::vec3{ 0.5f, 0.5f, 0.5f };
	mmesh.verts[2].pos = glm::vec3{ -0.5f, 0.5f, 0.5f };
	mmesh.verts[3].pos = glm::vec3{ -0.5f, -0.5f, 0.5f };
	mmesh.verts[4].pos = glm::vec3{ 0.5f, -0.5f, 0.5f };
	mmesh.verts[5].pos = glm::vec3{ 0.5f, 0.5f, 0.5f };


	mmesh.verts[0].col = glm::vec3{ 0.0f,0.0f,1.0f };
	mmesh.verts[1].col = glm::vec3{ 0.0f,1.0f,1.0f };
	mmesh.verts[2].col = glm::vec3{ 1.0f,1.0f,0.0f };
	mmesh.verts[3].col = glm::vec3{ 1.0f,0.0f,1.0f };
	mmesh.verts[4].col = glm::vec3{ 0.0f,1.0f,0.0f };
	mmesh.verts[5].col = glm::vec3{ 1.0f,1.0f,1.0f };


	mmesh.verts[0].uv = glm::vec2{ 0.0, 1.0 };
	mmesh.verts[1].uv = glm::vec2{ 1.0, 0.0 };
	mmesh.verts[2].uv = glm::vec2{ 0.0, 0.0 };
	mmesh.verts[3].uv = glm::vec2{ 0.0, 1.0 };
	mmesh.verts[4].uv = glm::vec2{ 1.0, 1.0 };
	mmesh.verts[5].uv = glm::vec2{ 1.0, 0.0 };


	/* back */
	mmesh.verts[6].pos = glm::vec3(-0.5f, -0.5f, -0.5f);
	mmesh.verts[7].pos = glm::vec3(-0.5f, 0.5f, -0.5f);
	mmesh.verts[8].pos = glm::vec3(0.5f, 0.5f, -0.5f);
	mmesh.verts[9].pos = glm::vec3(-0.5f, -0.5f, -0.5f);
	mmesh.verts[10].pos = glm::vec3(0.5f, 0.5f, -0.5f);
	mmesh.verts[11].pos = glm::vec3(0.5f, -0.5f, -0.5f);

	mmesh.verts[6].col = glm::vec3(0.5f, 1.0f, 0.5f);
	mmesh.verts[7].col = glm::vec3(0.5f, 1.0f, 0.5f);
	mmesh.verts[8].col = glm::vec3(0.5f, 1.0f, 0.5f);
	mmesh.verts[9].col = glm::vec3(0.5f, 1.0f, 0.5f);
	mmesh.verts[10].col = glm::vec3(0.5f, 1.0f, 0.5f);
	mmesh.verts[11].col = glm::vec3(0.5f, 1.0f, 0.5f);

	mmesh.verts[6].uv = glm::vec2(1.0, 1.0);
	mmesh.verts[7].uv = glm::vec2(1.0, 0.0);
	mmesh.verts[8].uv = glm::vec2(0.0, 0.0);
	mmesh.verts[9].uv = glm::vec2(1.0, 1.0);
	mmesh.verts[10].uv = glm::vec2(0.0, 0.0);
	mmesh.verts[11].uv = glm::vec2(0.0, 1.0);

	/* left */
	mmesh.verts[12].pos = glm::vec3(-0.5f, -0.5f, 0.5f);
	mmesh.verts[13].pos = glm::vec3(-0.5f, 0.5f, 0.5f);
	mmesh.verts[14].pos = glm::vec3(-0.5f, 0.5f, -0.5f);
	mmesh.verts[15].pos = glm::vec3(-0.5f, -0.5f, 0.5f);
	mmesh.verts[16].pos = glm::vec3(-0.5f, 0.5f, -0.5f);
	mmesh.verts[17].pos = glm::vec3(-0.5f, -0.5f, -0.5f);

	mmesh.verts[12].col = glm::vec3(0.5f, 0.5f, 1.0f);
	mmesh.verts[13].col = glm::vec3(0.5f, 0.5f, 1.0f);
	mmesh.verts[14].col = glm::vec3(0.5f, 0.5f, 1.0f);
	mmesh.verts[15].col = glm::vec3(0.5f, 0.5f, 1.0f);
	mmesh.verts[16].col = glm::vec3(0.5f, 0.5f, 1.0f);
	mmesh.verts[17].col = glm::vec3(0.5f, 0.5f, 1.0f);

	mmesh.verts[12].uv = glm::vec2(1.0, 1.0);
	mmesh.verts[13].uv = glm::vec2(1.0, 0.0);
	mmesh.verts[14].uv = glm::vec2(0.0, 0.0);
	mmesh.verts[15].uv = glm::vec2(1.0, 1.0);
	mmesh.verts[16].uv = glm::vec2(0.0, 0.0);
	mmesh.verts[17].uv = glm::vec2(0.0, 1.0);

	/* right */
	mmesh.verts[18].pos = glm::vec3(0.5f, -0.5f, 0.5f);
	mmesh.verts[19].pos = glm::vec3(0.5f, 0.5f, -0.5f);
	mmesh.verts[20].pos = glm::vec3(0.5f, 0.5f, 0.5f);
	mmesh.verts[21].pos = glm::vec3(0.5f, -0.5f, 0.5f);
	mmesh.verts[22].pos = glm::vec3(0.5f, -0.5f, -0.5f);
	mmesh.verts[23].pos = glm::vec3(0.5f, 0.5f, -0.5f);

	mmesh.verts[18].col = glm::vec3(0.0f, 0.5f, 0.5f);
	mmesh.verts[19].col = glm::vec3(0.0f, 0.5f, 0.5f);
	mmesh.verts[20].col = glm::vec3(0.0f, 0.5f, 0.5f);
	mmesh.verts[21].col = glm::vec3(0.0f, 0.5f, 0.5f);
	mmesh.verts[22].col = glm::vec3(0.0f, 0.5f, 0.5f);
	mmesh.verts[23].col = glm::vec3(0.0f, 0.5f, 0.5f);

	mmesh.verts[18].uv = glm::vec2(0.0, 1.0);
	mmesh.verts[19].uv = glm::vec2(1.0, 0.0);
	mmesh.verts[20].uv = glm::vec2(0.0, 0.0);
	mmesh.verts[21].uv = glm::vec2(0.0, 1.0);
	mmesh.verts[22].uv = glm::vec2(1.0, 1.0);
	mmesh.verts[23].uv = glm::vec2(1.0, 0.0);

	/* top */
	mmesh.verts[24].pos = glm::vec3(0.5f, 0.5f, 0.5f);
	mmesh.verts[25].pos = glm::vec3(-0.5f, 0.5f, -0.5f);
	mmesh.verts[26].pos = glm::vec3(-0.5f, 0.5f, 0.5f);
	mmesh.verts[27].pos = glm::vec3(0.5f, 0.5f, 0.5f);
	mmesh.verts[28].pos = glm::vec3(0.5f, 0.5f, -0.5f);
	mmesh.verts[29].pos = glm::vec3(-0.5f, 0.5f, -0.5f);

	mmesh.verts[24].col = glm::vec3(0.5f, 0.0f, 0.5f);
	mmesh.verts[25].col = glm::vec3(0.5f, 0.0f, 0.5f);
	mmesh.verts[26].col = glm::vec3(0.5f, 0.0f, 0.5f);
	mmesh.verts[27].col = glm::vec3(0.5f, 0.0f, 0.5f);
	mmesh.verts[28].col = glm::vec3(0.5f, 0.0f, 0.5f);
	mmesh.verts[29].col = glm::vec3(0.5f, 0.0f, 0.5f);

	mmesh.verts[24].uv = glm::vec2(0.0, 1.0);
	mmesh.verts[25].uv = glm::vec2(1.0, 0.0);
	mmesh.verts[26].uv = glm::vec2(0.0, 0.0);
	mmesh.verts[27].uv = glm::vec2(0.0, 1.0);
	mmesh.verts[28].uv = glm::vec2(1.0, 1.0);
	mmesh.verts[29].uv = glm::vec2(1.0, 0.0);

	/* bottom */
	mmesh.verts[30].pos = glm::vec3(0.5f, -0.5f, 0.5f);
	mmesh.verts[31].pos = glm::vec3(-0.5f, -0.5f, 0.5f);
	mmesh.verts[32].pos = glm::vec3(-0.5f, -0.5f, -0.5f);
	mmesh.verts[33].pos = glm::vec3(0.5f, -0.5f, 0.5f);
	mmesh.verts[34].pos = glm::vec3(-0.5f, -0.5f, -0.5f);
	mmesh.verts[35].pos = glm::vec3(0.5f, -0.5f, -0.5f);

	mmesh.verts[30].col = glm::vec3(0.5f, 0.5f, 0.0f);
	mmesh.verts[31].col = glm::vec3(0.5f, 0.5f, 0.0f);
	mmesh.verts[32].col = glm::vec3(0.5f, 0.5f, 0.0f);
	mmesh.verts[33].col = glm::vec3(0.5f, 0.5f, 0.0f);
	mmesh.verts[34].col = glm::vec3(0.5f, 0.5f, 0.0f);
	mmesh.verts[35].col = glm::vec3(0.5f, 0.5f, 0.0f);

	mmesh.verts[30].uv = glm::vec2(0.0, 0.0);
	mmesh.verts[31].uv = glm::vec2(0.0, 1.0);
	mmesh.verts[32].uv = glm::vec2(1.0, 1.0);
	mmesh.verts[33].uv = glm::vec2(0.0, 0.0);
	mmesh.verts[34].uv = glm::vec2(1.0, 1.0);
	mmesh.verts[35].uv = glm::vec2(1.0, 0.0);











}

