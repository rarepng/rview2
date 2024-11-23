#pragma once
#define GLM_ENABLE_EXPIREMENTAL
#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include "vkobjs.hpp"
#include "staticsettings.hpp"

class texinstance {
public:
	texinstance( glm::vec3 worldpos, bool randomize = false);
	~texinstance();

	void setinstancesettings(staticsettings settings);
	staticsettings& getinstancesettings();
	void checkforupdates();
	glm::mat4 calcmat();

	glm::vec2 getwpos();
	glm::quat getwrot();



private:

	//std::shared_ptr<vkgltfstatic> mgltfmodel = nullptr;
	staticsettings mmodelsettings{};

};
