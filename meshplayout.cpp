#include "meshplayout.hpp"

#include "vk/VkBootstrap.h"

bool meshplayout::init(vkobjs& mvkobjs, vktexdata& texdata, VkPipelineLayout& vkplayout) {



	VkPushConstantRange pCs{};
	pCs.offset = 0;
	pCs.size = sizeof(vkpushconstants);
	pCs.stageFlags = VK_SHADER_STAGE_ALL;
	//VkPushConstantRange pCs2{};
	//pCs.offset = sizeof(vkpushconstants);
	//pCs.size = sizeof(vkpushconstants);
	//pCs.stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT;

	VkDescriptorSetLayout layoutz[] = { texdata.texdescriptorlayout,mvkobjs.rdperspviewmatrixubo[0][0].rdubodescriptorlayout,mvkobjs.rdjointmatrixssbo[0].rdssbodescriptorlayout,};




	VkPipelineLayoutCreateInfo plinfo{};
	plinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plinfo.setLayoutCount = 4;
	plinfo.pSetLayouts = layoutz;
	plinfo.pushConstantRangeCount = 1;
	plinfo.pPushConstantRanges = &pCs;
	if (vkCreatePipelineLayout(mvkobjs.rdvkbdevice.device, &plinfo, nullptr, &vkplayout) != VK_SUCCESS)return false;
	return true;

}

void meshplayout::cleanup(vkobjs& mvkobjs, VkPipelineLayout& vkplayout)
{
	vkDestroyPipelineLayout(mvkobjs.rdvkbdevice.device, vkplayout, nullptr);
}
