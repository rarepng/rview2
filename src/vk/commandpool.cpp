#include "commandpool.hpp"
#include <VkBootstrap.h>

bool commandpool::init(vkobjs &rdata, VkCommandPool &vkpool) {
	VkCommandPoolCreateInfo poolcreateinfo{};
	poolcreateinfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolcreateinfo.queueFamilyIndex = rdata.vkdevice.get_queue_index(vkb::QueueType::graphics).value();
	poolcreateinfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	if (vkCreateCommandPool(rdata.vkdevice.device, &poolcreateinfo, nullptr, &vkpool) != VK_SUCCESS) {
		return false;
	}

	return true;
}

bool commandpool::initcompute(vkobjs &rdata, VkCommandPool &vkpool) {
	VkCommandPoolCreateInfo poolcreateinfo{};
	poolcreateinfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolcreateinfo.queueFamilyIndex = rdata.vkdevice.get_queue_index(vkb::QueueType::compute).value();
	poolcreateinfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	if (vkCreateCommandPool(rdata.vkdevice.device, &poolcreateinfo, nullptr, &vkpool) != VK_SUCCESS) {
		return false;
	}

	return true;
}
void commandpool::cleanup(vkobjs &rdata, VkCommandPool &vkpool) {
	vkDestroyCommandPool(rdata.vkdevice.device, vkpool, nullptr);
}
