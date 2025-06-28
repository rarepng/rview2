#pragma once
#include "vkobjs.hpp"
#include <string>
#include <vulkan/vulkan.h>

class pline {
public:
	static bool init(rvk &objs, VkPipelineLayout &playout, VkPipeline &pipeline, VkPrimitiveTopology topology,
	                 unsigned int v_in, unsigned int atts, std::vector<std::string> sfiles, bool char_or_short = false);
	static void cleanup(rvk &objs, VkPipeline &pipeline);
};
