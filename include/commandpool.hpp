#pragma once
#include "vkobjs.hpp"
#include <vulkan/vulkan.h>
class commandpool {
public:
	static bool init(vkobjs &rdata, VkCommandPool &vkpool);
	static bool initcompute(vkobjs &rdata, VkCommandPool &vkpool);
	static void cleanup(vkobjs &rdata, VkCommandPool &vkpool);
};