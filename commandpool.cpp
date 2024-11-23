#include "commandpool.hpp"
#include <vk/VkBootstrap.h>

bool commandpool::init(vkobjs& rdata) {
	VkCommandPoolCreateInfo poolcreateinfo{};
	poolcreateinfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolcreateinfo.queueFamilyIndex = rdata.rdvkbdevice.get_queue_index(vkb::QueueType::graphics).value();
	poolcreateinfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	if (vkCreateCommandPool(rdata.rdvkbdevice.device, &poolcreateinfo, nullptr, &rdata.rdcommandpool) != VK_SUCCESS) {
		return false;
	}

	return true;

}


void commandpool::cleanup(vkobjs& rdata) {
	vkDestroyCommandPool(rdata.rdvkbdevice.device, rdata.rdcommandpool, nullptr);
}
