#pragma once
#include "core/rvk.hpp"
#include <vulkan/vulkan.h>

namespace playout {
	bool init(rvkbucket &mvkobjs, VkPipelineLayout &vkplayout, std::span<VkDescriptorSetLayout> layoutz,
	                 size_t pushc_size);
	bool init_bindless(rvkbucket& objs);
	void cleanup(rvkbucket &mvkobjs, VkPipelineLayout &vkplayout);
};