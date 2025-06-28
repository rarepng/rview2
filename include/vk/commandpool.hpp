#pragma once
#include "vkobjs.hpp"
#include <vulkan/vulkan.h>
#include <VkBootstrap.h>

namespace commandpool {

static inline bool createsametype(rvk &rdata,const std::span<VkCommandPool> &vkpools,const vkb::QueueType& qtype) {
	for(auto& x:vkpools) {
		VkCommandPoolCreateInfo poolcreateinfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = rdata.vkdevice.get_queue_index(qtype).value()
		};

		if (vkCreateCommandPool(rdata.vkdevice.device, &poolcreateinfo, nullptr, &x) != VK_SUCCESS) {
			return false;
		}
	}

	return true;
}
static inline void destroy(rvk &rdata,const std::span<VkCommandPool> &vkpools) {
	for(auto& x:vkpools) {
		vkDestroyCommandPool(rdata.vkdevice.device, x, nullptr);
	}
}

};