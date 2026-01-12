#pragma once
#include "core/rvk.hpp"
#include <vulkan/vulkan.h>

class playout {
public:
	static bool init(rvkbucket &mvkobjs, VkPipelineLayout &vkplayout, std::span<VkDescriptorSetLayout> layoutz,
	                 size_t pushc_size);
	static void cleanup(rvkbucket &mvkobjs, VkPipelineLayout &vkplayout);
};