#pragma once
#include <core/rvk.hpp>
#include <vulkan/vulkan.h>
#include <VkBootstrap.h>
#include <span>

namespace commandpool {

	static inline bool createsametype(rvkbucket& rdata, const std::span<VkCommandPool>& vkpools, const vkb::QueueType& qtype) {
		const auto q_idx_opt = rdata.vkdevice.get_queue_index(qtype);

		if (!q_idx_opt.has_value()) [[unlikely]]
			return false;

		const VkDevice device = rdata.vkdevice.device;
		const VkCommandPoolCreateInfo pool_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
												   .pNext = nullptr,
												   .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
												   .queueFamilyIndex = *q_idx_opt};

		for (auto& pool : vkpools) {
			if (vkCreateCommandPool(device, &pool_info, nullptr, &pool) != VK_SUCCESS) [[unlikely]] {
				return false;
			}
			rdata.cleanupQ.push_function([device, pool]() { vkDestroyCommandPool(device, pool, nullptr); });
		}

		return true;
	}
};

namespace commandbuffer {

	static inline bool create(rvkbucket& rdata, VkCommandPool& vkpool, const std::span<VkCommandBuffer>& cbuffs) {
		VkCommandBufferAllocateInfo bufferallocinfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
													.commandPool = vkpool,
													.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, //not sure
													.commandBufferCount = static_cast<uint32_t>(cbuffs.size())};

		if (vkAllocateCommandBuffers(rdata.vkdevice.device, &bufferallocinfo, cbuffs.data()) != VK_SUCCESS) {
			return false;
		}

		return true;
	}

};
