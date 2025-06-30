#pragma once
#include "core/rvk.hpp"
#include <vulkan/vulkan.h>
#include <span>
namespace commandbuffer {

static inline bool create(rvk &rdata, VkCommandPool &vkpool,const std::span<VkCommandBuffer> &cbuffs) {
	VkCommandBufferAllocateInfo bufferallocinfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = vkpool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, //not sure
		.commandBufferCount = cbuffs.size()
	};
	if (vkAllocateCommandBuffers(rdata.vkdevice.device, &bufferallocinfo, cbuffs.data()) != VK_SUCCESS) {
		return false;
	}
	return true;
}

static inline void destroy(rvk &rdata, VkCommandPool &vkpool,const std::span<VkCommandBuffer> &cbuffs) {
	vkFreeCommandBuffers(rdata.vkdevice.device, vkpool, cbuffs.size(), cbuffs.data());
}

};