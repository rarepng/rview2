#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan.h>

#include "vkobjs.hpp"

class ssbomesh {
public:
	static bool init(vkobjs &objs, ssbodata &ssbodata);
	static void upload(vkobjs &objs, ssbodata &ssbodata, std::vector<glm::mat4> &mats);
	static void upload(vkobjs &objs, ssbodata &ssbodata, std::vector<glm::mat2x4> mats);
	static void cleanup(vkobjs &objs, ssbodata &ssbodata);
};