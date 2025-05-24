#pragma once
#include "vkobjs.hpp"
#include <vulkan/vulkan.h>

class framebuffer {
public:
	static bool init(vkobjs &rdata);
	static void cleanup(vkobjs &rdata);
};
