#pragma once
#include "vkobjs.hpp"
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
class ubo {
public:
	static bool init(vkobjs &mvkobjs, std::vector<ubodata> &ubodata);
	static void upload(vkobjs &mvkobjs, std::vector<ubodata> &ubodata, std::vector<glm::mat4> mats,
	                   unsigned int texidx);
	static void upload(vkobjs &mvkobjs, std::vector<ubodata> &ubodata, unsigned int texidx);
	static void cleanup(vkobjs &mvkobjs, std::vector<ubodata> &ubodata);
};
