#pragma once
#include <vulkan/vulkan.h>
#include "vkobjs.hpp"
class commandpool {
public:
	static bool init(vkobjs& rdata, VkCommandPool& vkpool);
	static void cleanup(vkobjs& rdata, VkCommandPool& vkpool);
};