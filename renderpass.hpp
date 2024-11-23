#pragma once
#include <vulkan/vulkan.h>
#include "vkobjs.hpp"
class renderpass {
public:
	static bool init(vkobjs& rdata);
	static void cleanup(vkobjs& rdata);
};