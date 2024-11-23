#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/dual_quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <cstdlib>
#include "texinstance.hpp"
texinstance::~texinstance() {}

texinstance::texinstance( glm::vec3 worldpos, bool randomize) {

	mmodelsettings.msworldpos = worldpos;


	checkforupdates();
}


void texinstance::checkforupdates() {
	static glm::vec3 worldPos = mmodelsettings.msworldpos;
	static glm::vec3 worldRot = mmodelsettings.msworldrot;
	static glm::vec3 worldScale = mmodelsettings.msworldscale;

	if (worldPos != mmodelsettings.msworldpos) {
		worldPos = mmodelsettings.msworldpos;
	}

	if (worldRot != mmodelsettings.msworldrot) {
		worldRot = mmodelsettings.msworldrot;
	}
	if (worldScale != mmodelsettings.msworldscale) {
		worldScale = mmodelsettings.msworldscale;
	}
}

glm::mat4 texinstance::calcmat() {
	glm::mat4 x{ 1.0 };
	//glm::mat4 t = glm::translate(x, mmodelsettings.msworldpos) * glm::scale(x, mmodelsettings.msworldscale) * glm::rotate(x, glm::radians(mmodelsettings.msworldrot.y), glm::vec3{ 0.0f,1.0f,0.0f });
	glm::mat4 t = glm::translate(x, mmodelsettings.msworldpos) * glm::scale(x, mmodelsettings.msworldscale) * glm::mat4_cast(glm::quat(glm::radians(mmodelsettings.msworldrot)));
	return t;
}



void texinstance::setinstancesettings(staticsettings settings) {
	mmodelsettings = settings;
}

staticsettings& texinstance::getinstancesettings() {
	return mmodelsettings;
}

glm::vec2 texinstance::getwpos() {
	return mmodelsettings.msworldpos;
}

glm::quat texinstance::getwrot() {
	return glm::normalize(glm::quat(glm::vec3(
		glm::radians(mmodelsettings.msworldrot.x),
		glm::radians(mmodelsettings.msworldrot.y),
		glm::radians(mmodelsettings.msworldrot.z)
	)));
}
