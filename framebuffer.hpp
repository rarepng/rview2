#pragma once
#include <vector>
#include <vulkan/vulkan.h>
#include "vkobjs.hpp"

class framebuffer {
public:
	static bool init(vkobjs& rdata);
	static void cleanup(vkobjs& rdata);
};