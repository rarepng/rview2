#pragma once
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "vkobjs.hpp"
class ubo {
public:
	static bool init(vkobjs& mvkobjs, std::vector<vkuniformbufferdata>& ubodata);
	static void upload(vkobjs& mvkobjs, std::vector<vkuniformbufferdata>& ubodata, std::vector<glm::mat4> mats, unsigned int texidx);
	static void upload(vkobjs& mvkobjs, std::vector<vkuniformbufferdata>& ubodata, unsigned int texidx);
	static void cleanup(vkobjs& mvkobjs, std::vector<vkuniformbufferdata>& ubodata);
};
