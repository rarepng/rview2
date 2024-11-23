#pragma once
#define GLM_ENABLE_EXPIREMENTAL
#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include "vkgltfstatic.hpp"
#include "vkobjs.hpp"
#include "staticsettings.hpp"

class staticinstance {
public:
	staticinstance(std::shared_ptr<vkgltfstatic>model, glm::vec2 worldpos, bool randomize = false);
	~staticinstance();

	void setinstancesettings(staticsettings settings);
	staticsettings getinstancesettings();
	void checkforupdates();

	glm::vec2 getwpos();
	glm::quat getwrot();



private:

	std::shared_ptr<vkgltfstatic> mgltfmodel = nullptr;
	staticsettings mmodelsettings{};

};