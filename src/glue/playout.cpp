#include "playout.hpp"

#include "VkBootstrap.h"

bool playout::init(rvk &mvkobjs, VkPipelineLayout &vkplayout, std::vector<VkDescriptorSetLayout> layoutz,
                   size_t pushc_size) {


	VkPushConstantRange pCs{};
	pCs.offset = 0;
	pCs.size = pushc_size;
	pCs.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkPipelineLayoutCreateInfo plinfo{};
	plinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plinfo.setLayoutCount = layoutz.size();
	plinfo.pSetLayouts = layoutz.data();
	plinfo.pushConstantRangeCount = 1; ////////////////////////////////
	plinfo.pPushConstantRanges = &pCs;
	if (vkCreatePipelineLayout(mvkobjs.vkdevice.device, &plinfo, nullptr, &vkplayout) != VK_SUCCESS)
		return false;
	return true;
}

void playout::cleanup(rvk &mvkobjs, VkPipelineLayout &vkplayout) {
	vkDestroyPipelineLayout(mvkobjs.vkdevice.device, vkplayout, nullptr);
}
