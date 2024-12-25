#pragma once
#include <vulkan/vulkan.h>
#include "vkobjs.hpp"

class playout {
public:
	static bool init(vkobjs& mvkobjs, VkPipelineLayout& vkplayout, std::vector<VkDescriptorSetLayout> layoutz, size_t pushc_size);
	static void cleanup(vkobjs& mvkobjs, VkPipelineLayout& vkplayout);
};