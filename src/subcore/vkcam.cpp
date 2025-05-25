#define _USE_MATH_DEFINES
#include "vkcam.hpp"
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

glm::mat4 vkcam::getview(vkobjs &mvkobjs) {
	mforward = glm::normalize(
	               glm::vec3{glm::sin(glm::radians(mvkobjs.azimuth)) * glm::cos(glm::radians(mvkobjs.elevation)),
	                         glm::sin(glm::radians(mvkobjs.elevation)),
	                         -glm::cos(glm::radians(mvkobjs.azimuth)) * glm::cos(glm::radians(mvkobjs.elevation))});

	mright = glm::normalize(glm::cross(mforward, wup));

	mup = glm::normalize(glm::cross(mright, mforward));

	mvkobjs.camwpos += mvkobjs.camfor * static_cast<float>(mvkobjs.tickdiff) * mforward +
	                     mvkobjs.camright * static_cast<float>(mvkobjs.tickdiff) * mright +
	                     mvkobjs.camup * static_cast<float>(mvkobjs.tickdiff) * mup;

	return glm::lookAt(mvkobjs.camwpos, mvkobjs.camwpos + mforward, mup);
}
