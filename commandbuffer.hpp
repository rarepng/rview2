#pragma once
#include "env.hpp"
#include "vkobjs.hpp"
class commandbuffer {
public:
	static bool init(vkobjs& rdata, VkCommandPool& vkpool, VkCommandBuffer& incommandbuffer);
	static void cleanup(vkobjs& rdata, VkCommandPool& vkpool, VkCommandBuffer& incommandbuffer);
};