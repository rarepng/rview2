#pragma once
#include "vkobjs.hpp"
#include <vulkan/vulkan.h>

class playout {
public:
	static bool init(rvk &mvkobjs, VkPipelineLayout &vkplayout, std::vector<VkDescriptorSetLayout> layoutz,
	                 size_t pushc_size);
	static void cleanup(rvk &mvkobjs, VkPipelineLayout &vkplayout);
};