#include <vector>
#include "gltfpipeline.hpp"
#include "vkshader.hpp"

#include <glm/glm.hpp>
#include <vk/VkBootstrap.h>

bool gltfpipeline::init(vkobjs& objs, VkPipelineLayout& playout, VkPipeline& pipeline, VkPrimitiveTopology topology, std::string vname, std::string fname)
{
	VkShaderModule vmod = vkshader::loadshader(objs.rdvkbdevice.device, vname);
	VkShaderModule fmod = vkshader::loadshader(objs.rdvkbdevice.device, fname);

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




	VkVertexInputBindingDescription vbinding[3];
	vbinding[0].binding = 0;
	vbinding[0].stride = sizeof(glm::vec3);
	vbinding[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	vbinding[1].binding = 1;
	vbinding[1].stride = sizeof(glm::vec3);
	vbinding[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	vbinding[2].binding = 2;
	vbinding[2].stride = sizeof(glm::vec2);
	vbinding[2].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;



	//VkVertexInputBindingDescription mainbind{};
	//mainbind.binding = 0;
	//mainbind.stride = sizeof(vkvert);
	//mainbind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription posa{};
	posa.binding = 0;
	posa.location = 0;
	posa.format = VK_FORMAT_R32G32B32_SFLOAT;
	posa.offset = 0;


	VkVertexInputAttributeDescription cola{};
	cola.binding = 1;
	cola.location = 1;
	cola.format = VK_FORMAT_R32G32B32_SFLOAT;
	cola.offset = 0;


	VkVertexInputAttributeDescription uva{};
	uva.binding = 2;
	uva.location = 2;
	uva.format = VK_FORMAT_R32G32_SFLOAT;
	uva.offset = 0;

	VkVertexInputAttributeDescription atts[] = { posa,cola,uva };

	VkPipelineVertexInputStateCreateInfo vertexinputinfo{};
	vertexinputinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexinputinfo.vertexBindingDescriptionCount = 3;
	vertexinputinfo.pVertexBindingDescriptions = vbinding;
	vertexinputinfo.vertexAttributeDescriptionCount = 3;
	vertexinputinfo.pVertexAttributeDescriptions = atts;

	VkPipelineInputAssemblyStateCreateInfo inputassemblyinfo{};
	inputassemblyinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputassemblyinfo.topology = topology;
	inputassemblyinfo.primitiveRestartEnable = VK_FALSE;



	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(objs.rdvkbswapchain.extent.width);
	viewport.height = static_cast<float>(objs.rdvkbswapchain.extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = { 0,0 };
	scissor.extent = objs.rdvkbswapchain.extent;

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
	pipelinecreateinfo.layout = playout;
	pipelinecreateinfo.renderPass = objs.rdrenderpass;
	pipelinecreateinfo.subpass = 0;
	pipelinecreateinfo.basePipelineHandle = VK_NULL_HANDLE;


	if (vkCreateGraphicsPipelines(objs.rdvkbdevice.device, VK_NULL_HANDLE, 1, &pipelinecreateinfo, nullptr, &pipeline) != VK_SUCCESS) {
		vkDestroyPipelineLayout(objs.rdvkbdevice.device, playout, nullptr);
		return false;
	}
	vkDestroyShaderModule(objs.rdvkbdevice.device, fmod, nullptr);
	vkDestroyShaderModule(objs.rdvkbdevice.device, vmod, nullptr);

	return true;
}







void gltfpipeline::cleanup(vkobjs& objs, VkPipeline& pipeline) {
    vkDestroyPipeline(objs.rdvkbdevice.device, pipeline, nullptr);
}