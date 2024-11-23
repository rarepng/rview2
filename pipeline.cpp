#include <vector>
#include <vk/VkBootstrap.h>
#include "pipeline.hpp"
#include "vkshader.hpp"

bool pipeline::init(vkobjs& rdata, VkPipelineLayout& vkplayout, VkPipeline& vkpipe, VkPrimitiveTopology topology, std::string v, std::string f) {



	VkShaderModule vmod = vkshader::loadshader(rdata.rdvkbdevice.device, v);
	VkShaderModule fmod = vkshader::loadshader(rdata.rdvkbdevice.device, f);

	if (vmod == VK_NULL_HANDLE || fmod == VK_NULL_HANDLE) {
		return false;
	}

	VkPipelineShaderStageCreateInfo vertexstageinfo{};
	vertexstageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexstageinfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexstageinfo.module = vmod;
	vertexstageinfo.pName = "main";


	VkPipelineShaderStageCreateInfo fragstageinfo{};
	fragstageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragstageinfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragstageinfo.module = fmod;
	fragstageinfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderstagesinfo[] = { vertexstageinfo,fragstageinfo };








	VkVertexInputBindingDescription mainbind{};
	mainbind.binding = 0;
	mainbind.stride = sizeof(vkvert);
	mainbind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription posa{};
	posa.binding = 0;
	posa.location = 0;
	posa.format = VK_FORMAT_R32G32B32_SFLOAT;
	posa.offset = offsetof(vkvert, pos);


	VkVertexInputAttributeDescription cola{};
	cola.binding = 0;
	cola.location = 1;
	cola.format = VK_FORMAT_R32G32B32_SFLOAT;
	cola.offset = offsetof(vkvert, col);


	VkVertexInputAttributeDescription uva{};
	uva.binding = 0;
	uva.location = 2;
	uva.format = VK_FORMAT_R32G32_SFLOAT;
	uva.offset = offsetof(vkvert, uv);

	VkVertexInputAttributeDescription atts[] = { posa,cola,uva };

	VkPipelineVertexInputStateCreateInfo vertexinputinfo{};
	vertexinputinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexinputinfo.vertexBindingDescriptionCount = 1;
	vertexinputinfo.pVertexBindingDescriptions = &mainbind;
	vertexinputinfo.vertexAttributeDescriptionCount = 3;
	vertexinputinfo.pVertexAttributeDescriptions = atts;

	VkPipelineInputAssemblyStateCreateInfo inputassemblyinfo{};
	inputassemblyinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputassemblyinfo.topology = topology;
	inputassemblyinfo.primitiveRestartEnable = VK_FALSE;



	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(rdata.rdvkbswapchain.extent.width);
	viewport.height = static_cast<float>(rdata.rdvkbswapchain.extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 0.0f;

	VkRect2D scissor{};
	scissor.offset = { 0,0 };
	scissor.extent = rdata.rdvkbswapchain.extent;

	VkPipelineViewportStateCreateInfo viewportstateinfo{};
	viewportstateinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportstateinfo.viewportCount = 1;
	viewportstateinfo.pViewports = &viewport;
	viewportstateinfo.scissorCount = 1;
	viewportstateinfo.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizerinfo{};
	rasterizerinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerinfo.depthClampEnable = VK_FALSE;
	rasterizerinfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizerinfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizerinfo.lineWidth = 1.0f;
	rasterizerinfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizerinfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizerinfo.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisampleinfo{};
	multisampleinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleinfo.sampleShadingEnable = VK_FALSE;
	multisampleinfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colorblenda{};
	colorblenda.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorblenda.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorblendinginfo{};
	colorblendinginfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorblendinginfo.logicOpEnable = VK_FALSE;
	colorblendinginfo.logicOp = VK_LOGIC_OP_COPY;
	colorblendinginfo.attachmentCount = 1;
	colorblendinginfo.pAttachments = &colorblenda;
	colorblendinginfo.blendConstants[0] = 0.0f;
	colorblendinginfo.blendConstants[1] = 0.0f;
	colorblendinginfo.blendConstants[2] = 0.0f;
	colorblendinginfo.blendConstants[3] = 0.0f;

	VkPipelineDepthStencilStateCreateInfo depthstencilinfo{};
	depthstencilinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthstencilinfo.depthTestEnable = VK_TRUE;
	depthstencilinfo.depthWriteEnable = VK_TRUE;
	depthstencilinfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthstencilinfo.depthBoundsTestEnable = VK_FALSE;
	depthstencilinfo.minDepthBounds = 0.0f;
	depthstencilinfo.maxDepthBounds = 1.0f;
	depthstencilinfo.stencilTestEnable = VK_FALSE;

	std::vector<VkDynamicState> dynstates = { VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_LINE_WIDTH };


	VkPipelineDynamicStateCreateInfo dynstateinfo{};
	dynstateinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynstateinfo.dynamicStateCount = static_cast<uint32_t>(dynstates.size());
	dynstateinfo.pDynamicStates = dynstates.data();

	VkGraphicsPipelineCreateInfo pipelinecreateinfo{};
	pipelinecreateinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelinecreateinfo.stageCount = 2;
	pipelinecreateinfo.pStages = shaderstagesinfo;
	pipelinecreateinfo.pVertexInputState = &vertexinputinfo;
	pipelinecreateinfo.pInputAssemblyState = &inputassemblyinfo;
	pipelinecreateinfo.pViewportState = &viewportstateinfo;
	pipelinecreateinfo.pRasterizationState = &rasterizerinfo;
	pipelinecreateinfo.pMultisampleState = &multisampleinfo;
	pipelinecreateinfo.pColorBlendState = &colorblendinginfo;
	pipelinecreateinfo.pDepthStencilState = &depthstencilinfo;
	pipelinecreateinfo.pDynamicState = &dynstateinfo;
	pipelinecreateinfo.layout = vkplayout;
	pipelinecreateinfo.renderPass = rdata.rdrenderpass;
	pipelinecreateinfo.subpass = 0;
	pipelinecreateinfo.basePipelineHandle = VK_NULL_HANDLE;


	if (vkCreateGraphicsPipelines(rdata.rdvkbdevice.device, VK_NULL_HANDLE, 1, &pipelinecreateinfo, nullptr, &vkpipe) != VK_SUCCESS) {
		vkDestroyPipelineLayout(rdata.rdvkbdevice.device, vkplayout, nullptr);
		return false;
	}
	vkDestroyShaderModule(rdata.rdvkbdevice.device, fmod, nullptr);
	vkDestroyShaderModule(rdata.rdvkbdevice.device, vmod, nullptr);

	return true;

}
void pipeline::cleanup(vkobjs& rdata,VkPipeline& vkpipe) {
	vkDestroyPipeline(rdata.rdvkbdevice.device,vkpipe, nullptr);
}