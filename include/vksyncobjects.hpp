#pragma once
#include "vkobjs.hpp"
#include <VkBootstrap.h>
#include <vulkan/vulkan.h>
class vksyncobjects {
public:
	static bool init(vkobjs &rdata);
	static void cleanup(vkobjs &rdata);
};
