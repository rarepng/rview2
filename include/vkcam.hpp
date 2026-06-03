#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#define _USE_MATH_DEFINES
// #include <cmath>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

#include "core/rvk.hpp"
#include <glm/glm.hpp>

namespace vkcam {
inline glm::mat4 getview(rvkbucket &mvkobjs) {
	mvkobjs.mvkcam.mforward = glm::normalize(
	                              glm::vec3{glm::sin(glm::radians(mvkobjs.azimuth)) * glm::cos(glm::radians(mvkobjs.elevation)),
	                                        glm::sin(glm::radians(mvkobjs.elevation)),
	                                        -glm::cos(glm::radians(mvkobjs.azimuth)) * glm::cos(glm::radians(mvkobjs.elevation))});

	mvkobjs.mvkcam.mright = glm::normalize(glm::cross(mvkobjs.mvkcam.mforward, mvkobjs.mvkcam.wup));

	mvkobjs.mvkcam.mup = glm::normalize(glm::cross(mvkobjs.mvkcam.mright, mvkobjs.mvkcam.mforward));

	mvkobjs.camwpos += mvkobjs.camfor * static_cast<float>(mvkobjs.tickdiff) * mvkobjs.mvkcam.mforward +
	                   mvkobjs.camright * static_cast<float>(mvkobjs.tickdiff) * mvkobjs.mvkcam.mright +
	                   mvkobjs.camup * static_cast<float>(mvkobjs.tickdiff) * mvkobjs.mvkcam.mup;

	return glm::lookAt(mvkobjs.camwpos, mvkobjs.camwpos + mvkobjs.mvkcam.mforward, mvkobjs.mvkcam.mup);
}
};