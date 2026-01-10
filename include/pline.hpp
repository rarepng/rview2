#pragma once
#include "core/rvk.hpp"
#include <string>
#include <vulkan/vulkan.h>

class pline {
public:
	static bool init(rvk &objs, VkPipelineLayout &playout, VkPipeline &pipeline, VkPrimitiveTopology topology,
	                 unsigned int v_in, unsigned int atts, std::vector<std::string> sfiles, bool char_or_short = false,VkFormat cformat=VK_FORMAT_R32G32B32A32_SFLOAT, VkFormat dformat=VK_FORMAT_D32_SFLOAT);
	static void cleanup(rvk &objs, VkPipeline &pipeline);
};
