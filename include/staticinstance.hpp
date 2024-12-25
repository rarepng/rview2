#pragma once
#define GLM_ENABLE_EXPIREMENTAL
#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include "staticmodel.hpp"
#include "vkobjs.hpp"
#include "staticsettings.hpp"

class staticinstance {
public:
	staticinstance(std::shared_ptr<staticmodel>model, glm::vec3 worldpos, bool randomize = false);
	~staticinstance();

	void setinstancesettings(staticsettings settings);
	staticsettings& getinstancesettings();
	void checkforupdates();
	glm::mat4 calcmat();

	glm::vec2 getwpos();
	glm::quat getwrot();



private:

	std::shared_ptr<staticmodel> mgltfmodel = nullptr;
	staticsettings mmodelsettings{};

};
