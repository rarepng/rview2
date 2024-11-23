#pragma once
#include <vector>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include "vkobjs.hpp"

class ssbo {
public:
	static bool init(vkobjs& objs, vkshaderstoragebufferdata& ssbodata, size_t buffersize);
	static void upload(vkobjs& objs, vkshaderstoragebufferdata& ssbodata,std::vector<glm::mat4> mats);
	static void upload(vkobjs& objs, vkshaderstoragebufferdata& ssbodata,std::vector<glm::mat2x4> mats);
	static void cleanup(vkobjs& objs, vkshaderstoragebufferdata& ssbodata);
};