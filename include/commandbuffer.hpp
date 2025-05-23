#pragma once
#include "vkobjs.hpp"
#include <vulkan/vulkan.h>
class commandbuffer {
public:
	static bool init(vkobjs &rdata, VkCommandPool &vkpool, VkCommandBuffer &incommandbuffer);
	static void cleanup(vkobjs &rdata, VkCommandPool &vkpool, VkCommandBuffer &incommandbuffer);
};