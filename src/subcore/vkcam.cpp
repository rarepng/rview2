#define _USE_MATH_DEFINES
#include "vkcam.hpp"
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

glm::mat4 vkcam::getview(vkobjs &mvkobjs) {
	mforward = glm::normalize(
	               glm::vec3{glm::sin(glm::radians(mvkobjs.rdazimuth)) * glm::cos(glm::radians(mvkobjs.rdelevation)),
	                         glm::sin(glm::radians(mvkobjs.rdelevation)),
	                         -glm::cos(glm::radians(mvkobjs.rdazimuth)) * glm::cos(glm::radians(mvkobjs.rdelevation))});

	mright = glm::normalize(glm::cross(mforward, wup));

	mup = glm::normalize(glm::cross(mright, mforward));

	mvkobjs.rdcamwpos += mvkobjs.rdcamforward * static_cast<float>(mvkobjs.tickdiff) * mforward +
	                     mvkobjs.rdcamright * static_cast<float>(mvkobjs.tickdiff) * mright +
	                     mvkobjs.rdcamup * static_cast<float>(mvkobjs.tickdiff) * mup;

	return glm::lookAt(mvkobjs.rdcamwpos, mvkobjs.rdcamwpos + mforward, mup);
}
