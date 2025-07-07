#include "vksyncobjects.hpp"

bool vksyncobjects::init(rvk &rdata) {

	VkFenceCreateInfo fenceinfo{};
	fenceinfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceinfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkSemaphoreCreateInfo semaphoreinfo{};
	semaphoreinfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	if (vkCreateSemaphore(rdata.vkdevice.device, &semaphoreinfo, nullptr, &rdata.presentsemaphore) != VK_SUCCESS ||
	        vkCreateSemaphore(rdata.vkdevice.device, &semaphoreinfo, nullptr, &rdata.rendersemaphore) != VK_SUCCESS ||
	        vkCreateFence(rdata.vkdevice.device, &fenceinfo, nullptr, &rdata.renderfence) != VK_SUCCESS ||
	        vkCreateFence(rdata.vkdevice.device, &fenceinfo, nullptr, &rdata.uploadfence) != VK_SUCCESS) {
		return false;
	}
	return true;
}
void vksyncobjects::cleanup(rvk &rdata) {
	vkDestroySemaphore(rdata.vkdevice.device, rdata.presentsemaphore, nullptr);
	vkDestroySemaphore(rdata.vkdevice.device, rdata.rendersemaphore, nullptr);
	vkDestroyFence(rdata.vkdevice.device, rdata.renderfence, nullptr);
	vkDestroyFence(rdata.vkdevice.device, rdata.uploadfence, nullptr);
}
