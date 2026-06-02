#pragma once
#include "core/rvk.hpp"
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
namespace ubo {
	bool init_global(rvkbucket &mvkobjs);
	void upload(rvkbucket &mvkobjs, std::vector<ssbodata> &ubodata, std::vector<glm::mat4> mats,const glm::vec3& campos);
	void cleanup(rvkbucket &mvkobjs, std::vector<ssbodata> &ubodata);
};
