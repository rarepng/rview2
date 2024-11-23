#include "playout.hpp"

#include "vk/VkBootstrap.h"

bool playout::init(vkobjs& mvkobjs, VkPipelineLayout& vkplayout, std::vector<VkDescriptorSetLayout> layoutz,size_t pushc_size) {

	size_t totalcount{ 0 };

	VkPushConstantRange pCs{};
	pCs.offset = 0;
	pCs.size = pushc_size;
	pCs.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;


	VkPipelineLayoutCreateInfo plinfo{};
	plinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plinfo.setLayoutCount = layoutz.size();
	plinfo.pSetLayouts = layoutz.data();
	plinfo.pushConstantRangeCount = 1;////////////////////////////////
	plinfo.pPushConstantRanges = &pCs;
	if (vkCreatePipelineLayout(mvkobjs.rdvkbdevice.device, &plinfo, nullptr, &vkplayout) != VK_SUCCESS)return false;
	return true;

}

void playout::cleanup(vkobjs& mvkobjs, VkPipelineLayout& vkplayout)
{
	vkDestroyPipelineLayout(mvkobjs.rdvkbdevice.device, vkplayout, nullptr);
}
