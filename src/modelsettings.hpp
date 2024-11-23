
#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>

struct modelsettings {
	glm::vec3 msworldpos{ 0.0f };
	glm::vec3 msworldrot{ 0.0f };
	glm::vec3 msworldscale{ 1.0f };
	bool msdrawmodel{ true };
	bool animloop{ true };
	bool msdrawskeleton{ false };
	skinningmode mvertexskinningmode = skinningmode::linear;
	bool msplayanimation{ true };
	replaydirection msanimationplaydirection = replaydirection::forward;
	int msanimclip{ 0 };

	bool dead{ false };

	double animstart{ 0.0 };

	float msanimspeed{ 1.0f };
	float msanimtimepos{ 0.0f };
	float msanimendtime{ 0.0f };
	blendmode msblendingmode = blendmode::fadeinout;
	float msanimblendfactor{ 1.0f };
	int mscrossblenddestanimclip{ 0 };
	float msanimcrossblendfactor{ 0.0f };
	int msskelsplitnode{ 0 };
	std::vector<std::string> msclipnames{};
	std::vector<std::string> msskelnodenames{};
	ikmode msikmode = ikmode::off;
	int msikiterations{ 10 };
	glm::vec3 msiktargetpos{ 0.0f,3.0f,1.0f };
	glm::vec3 msiktargetworldpos{ 0.0f,0.0f,1.0f };
	int msikeffectornode{ 0 };
	int msikrootnode{ 0 };
	double hp{ 1.0 };

};
