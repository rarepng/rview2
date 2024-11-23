#pragma once
#include "env.hpp"
#include "vkobjs.hpp"
class commandbuffer {
public:
	static bool init(vkobjs& rdata, VkCommandBuffer& incommandbuffer);
	static void cleanup(vkobjs& rdata, VkCommandBuffer& incommandbuffer);
};