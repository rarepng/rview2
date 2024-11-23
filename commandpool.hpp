#pragma once
#include <vulkan/vulkan.h>
#include "vkobjs.hpp"
class commandpool {
public:
	static bool init(vkobjs& rdata);
	static void cleanup(vkobjs& rdata);
};