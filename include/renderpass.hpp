#pragma once
#include "vkobjs.hpp"
#include <vulkan/vulkan.h>
class renderpass {
public:
	static bool init(vkobjs &rdata);
	static void cleanup(vkobjs &rdata);
};
