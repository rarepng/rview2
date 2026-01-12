#include "vksyncobjects.hpp"
#include <algorithm>
#include <tuple>

bool vksyncobjects::init(rvkbucket &rdata) {

	VkFenceCreateInfo fenceinfo{};
	fenceinfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceinfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkSemaphoreCreateInfo semaphoreinfo{};
	semaphoreinfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;


	return std::apply([](auto &&... tasks) {
		return (... && tasks());
	},
	std::tuple{
		[&] {
			return std::ranges::all_of(rdata.semaphorez, [&](auto& sem_row) {
				return std::ranges::all_of(sem_row, [&](VkSemaphore& sem) {
					return vkCreateSemaphore(rdata.vkdevice.device, &semaphoreinfo, nullptr, &sem) == VK_SUCCESS;
				});
			});
		},
		[&] {
			return std::ranges::all_of(rdata.fencez, [&](VkFence& fence) {
				return vkCreateFence(rdata.vkdevice.device, &fenceinfo, nullptr, &fence) == VK_SUCCESS;
			});
		}
	}
	                 );
}
void vksyncobjects::cleanup(rvkbucket &rdata) {
	for(const auto& x:rdata.semaphorez) {
		for(const auto& y:x) {
			vkDestroySemaphore(rdata.vkdevice.device, y, nullptr);
		}
	}
	for(const auto& x:rdata.fencez) {
		vkDestroyFence(rdata.vkdevice.device, x, nullptr);
	}
}
