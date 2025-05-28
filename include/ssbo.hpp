#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan.h>

#include "vkobjs.hpp"

class ssbo {
public:
	static bool init(vkobjs &objs, ssbodata &ssbodata, size_t buffersize);
	template<typename T>
	static void upload(const vkobjs &objs,const ssbodata &ssbodata, const std::vector<T> &mats);
	static void cleanup(vkobjs &objs, ssbodata &ssbodata);
};
