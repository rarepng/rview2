#include "syncobjects.hpp"

#include <vk/VkBootstrap.h>

bool syncobjects::init(vkobjs& rdata) {

	VkFenceCreateInfo fenceinfo{};
	fenceinfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceinfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkSemaphoreCreateInfo semaphoreinfo{};
	semaphoreinfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	if (vkCreateSemaphore(rdata.rdvkbdevice.device, &semaphoreinfo, nullptr, &rdata.rdpresentsemaphore) != VK_SUCCESS ||
		vkCreateSemaphore(rdata.rdvkbdevice.device, &semaphoreinfo, nullptr, &rdata.rdrendersemaphore) != VK_SUCCESS ||
		vkCreateFence(rdata.rdvkbdevice.device, &fenceinfo, nullptr, &rdata.rdrenderfence) != VK_SUCCESS) {
		return false;
	}



	return true;
}
void syncobjects::cleanup(vkobjs& rdata) {
	vkDestroySemaphore(rdata.rdvkbdevice.device, rdata.rdpresentsemaphore, nullptr);
	vkDestroySemaphore(rdata.rdvkbdevice.device, rdata.rdrendersemaphore, nullptr);
	vkDestroyFence(rdata.rdvkbdevice.device, rdata.rdrenderfence, nullptr);
}