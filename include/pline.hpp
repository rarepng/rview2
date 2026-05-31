#pragma once
#include "core/rvk.hpp"
#include <string>
#include <vulkan/vulkan.h>

namespace pline {
	bool init(rvkbucket &mvkobjs, VkPipelineLayout &playout, VkPipeline &pipeline, VkPrimitiveTopology topology,
	                 unsigned int v_in, unsigned int atts, std::vector<std::string> sfiles, bool char_or_short = false,VkFormat cformat=VK_FORMAT_R32G32B32A32_SFLOAT, VkFormat dformat=VK_FORMAT_D32_SFLOAT);
	bool initcompute(rvkbucket &mvkobjs, VkPipelineLayout &playout, VkPipeline &pipeline, std::vector<std::string> sfiles = {});
	void cleanup(rvkbucket &mvkobjs, VkPipeline &pipeline);
};
