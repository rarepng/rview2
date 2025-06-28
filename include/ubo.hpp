#pragma once
#include "vkobjs.hpp"
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
class ubo {
public:
	static bool init(rvk &mvkobjs, std::vector<ubodata> &ubodata);
	static void upload(rvk &mvkobjs, std::vector<ubodata> &ubodata, std::vector<glm::mat4> mats);
	static void cleanup(rvk &mvkobjs, std::vector<ubodata> &ubodata);
};
