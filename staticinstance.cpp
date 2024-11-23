#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/dual_quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <cstdlib>
#include "staticinstance.hpp"
staticinstance::~staticinstance() {}

staticinstance::staticinstance(std::shared_ptr<vkgltfstatic> model, glm::vec2 worldpos, bool randomize) {

	if (!model)return;
	mgltfmodel = model;
	mmodelsettings.msworldpos = worldpos;

	if (randomize) {
		float initrotation = std::rand() % 360 - 180;

		mmodelsettings.msworldrot = glm::vec3(0.0f, initrotation, 0.0f);

	}

	checkforupdates();
}


void staticinstance::checkforupdates() {
	static glm::vec2 worldPos = mmodelsettings.msworldpos;
	static glm::vec3 worldRot = mmodelsettings.msworldrot;

	if (worldPos != mmodelsettings.msworldpos) {
		worldPos = mmodelsettings.msworldpos;
	}

	if (worldRot != mmodelsettings.msworldrot) {
		worldRot = mmodelsettings.msworldrot;
	}
}



void staticinstance::setinstancesettings(staticsettings settings) {
	mmodelsettings = settings;
}

staticsettings staticinstance::getinstancesettings() {
	return mmodelsettings;
}

glm::vec2 staticinstance::getwpos() {
	return mmodelsettings.msworldpos;
}

glm::quat staticinstance::getwrot() {
	return glm::normalize(glm::quat(glm::vec3(
		glm::radians(mmodelsettings.msworldrot.x),
		glm::radians(mmodelsettings.msworldrot.y),
		glm::radians(mmodelsettings.msworldrot.z)
	)));
}
