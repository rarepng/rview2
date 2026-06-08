#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#define _USE_MATH_DEFINES
// #include <cmath>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

#include "core/rvk.hpp"
#include <glm/glm.hpp>

namespace vkcam {
inline glm::mat4 getview() {
	rview::core::mvkcam.mforward = glm::normalize(
	                                   glm::vec3{glm::sin(glm::radians(rview::core::azimuth)) * glm::cos(glm::radians(rview::core::elevation)),
	                                       glm::sin(glm::radians(rview::core::elevation)),
	                                       -glm::cos(glm::radians(rview::core::azimuth)) * glm::cos(glm::radians(rview::core::elevation))});

	rview::core::mvkcam.mright = glm::normalize(glm::cross(rview::core::mvkcam.mforward, rview::core::mvkcam.wup));

	rview::core::mvkcam.mup = glm::normalize(glm::cross(rview::core::mvkcam.mright, rview::core::mvkcam.mforward));

	rview::core::camwpos += rview::core::camfor * static_cast<float>(rview::core::tickdiff) * rview::core::mvkcam.mforward +
	                        rview::core::camright * static_cast<float>(rview::core::tickdiff) * rview::core::mvkcam.mright +
	                        rview::core::camup * static_cast<float>(rview::core::tickdiff) * rview::core::mvkcam.mup;

	return glm::lookAt(rview::core::camwpos, rview::core::camwpos + rview::core::mvkcam.mforward, rview::core::mvkcam.mup);
}
};