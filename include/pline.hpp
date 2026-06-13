#pragma once
#include <core/rvk.hpp>
#include <string>
#include <vulkan/vulkan.h>
#include <vector>
#include <vkshader.hpp>
#include <VkBootstrap.h>
#include <glm/glm.hpp>

namespace pline {
inline bool init(rvkbucket &mvkobjs, VkPipelineLayout &playout, VkPipeline &pipeline, VkPrimitiveTopology topology,
                 std::vector<std::string_view> sfiles, VkFormat cformat, VkFormat dformat) {
	if (sfiles.size() < 2)
		return false;

	std::vector<VkShaderModule> shaders;
	std::vector<VkPipelineShaderStageCreateInfo> shaderStageInfo;
	shaders.reserve(sfiles.size());
	shaders.resize(sfiles.size());
	shaderStageInfo.reserve(sfiles.size());
	shaderStageInfo.resize(sfiles.size());

	for (size_t i{0}; i < sfiles.size(); i++) {
		shaders[i] = vkshader::loadshader(mvkobjs.vkdevice.device, sfiles[i]);
		shaderStageInfo[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageInfo[i].module = shaders[i];
		shaderStageInfo[i].pName = "main";
	}

	shaderStageInfo.front().stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStageInfo.back().stage = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
	inputAssemblyInfo.topology = topology;
	inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportStateInfo{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
	viewportStateInfo.viewportCount = 1;
	viewportStateInfo.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterizerInfo{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
	rasterizerInfo.depthClampEnable = VK_FALSE;
	rasterizerInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizerInfo.lineWidth = 1.0f;
	rasterizerInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizerInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizerInfo.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisamplingInfo{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
	multisamplingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlendingInfo{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
	colorBlendingInfo.attachmentCount = 1;
	colorBlendingInfo.pAttachments = &colorBlendAttachment;

	VkPipelineDepthStencilStateCreateInfo depthStencilInfo{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
	depthStencilInfo.depthTestEnable = VK_TRUE;
	depthStencilInfo.depthWriteEnable = VK_TRUE;
	depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencilInfo.minDepthBounds = 0.0f;
	depthStencilInfo.maxDepthBounds = 1.0f;

	std::vector<VkDynamicState> dynStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_LINE_WIDTH};
	VkPipelineDynamicStateCreateInfo dynStatesInfo{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
	dynStatesInfo.dynamicStateCount = static_cast<uint32_t>(dynStates.size());
	dynStatesInfo.pDynamicStates = dynStates.data();

	VkPipelineRenderingCreateInfo plinerenderinginfo{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
	plinerenderinginfo.colorAttachmentCount = 1;
	plinerenderinginfo.pColorAttachmentFormats = &cformat;
	plinerenderinginfo.depthAttachmentFormat = dformat;

	VkGraphicsPipelineCreateInfo pipelineCreateInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
	pipelineCreateInfo.pNext = &plinerenderinginfo;
	pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStageInfo.size());
	pipelineCreateInfo.pStages = shaderStageInfo.data();
	pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyInfo;
	pipelineCreateInfo.pViewportState = &viewportStateInfo;
	pipelineCreateInfo.pRasterizationState = &rasterizerInfo;
	pipelineCreateInfo.pMultisampleState = &multisamplingInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendingInfo;
	pipelineCreateInfo.pDepthStencilState = &depthStencilInfo;
	pipelineCreateInfo.pDynamicState = &dynStatesInfo;
	pipelineCreateInfo.layout = playout;

	if (vkCreateGraphicsPipelines(mvkobjs.vkdevice.device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline) != VK_SUCCESS) {
		vkDestroyPipelineLayout(mvkobjs.vkdevice.device, playout, nullptr);
		return false;
	}

	for (const auto &s : shaders) {
		vkDestroyShaderModule(mvkobjs.vkdevice.device, s, nullptr);
	}

	return true;
}
inline bool initcompute(rvkbucket &mvkobjs, VkPipelineLayout &playout, VkPipeline &pipeline, std::vector<std::string_view> sfiles) {
	if (sfiles.size() < 1)
		return false;

	std::vector<VkShaderModule> shaders;
	std::vector<VkPipelineShaderStageCreateInfo> shaderStageInfo;
	std::vector<VkComputePipelineCreateInfo> computePipelineCreateInfo;
	shaders.reserve(sfiles.size());
	shaders.resize(sfiles.size());
	shaderStageInfo.reserve(sfiles.size());
	shaderStageInfo.resize(sfiles.size());
	computePipelineCreateInfo.reserve(sfiles.size());
	computePipelineCreateInfo.resize(sfiles.size());

	for (size_t i{0}; i < sfiles.size(); i++) {
		shaders[i] = vkshader::loadshader(mvkobjs.vkdevice.device, sfiles[i]);

		shaderStageInfo[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageInfo[i].module = shaders[i];
		shaderStageInfo[i].pName = "main";
		shaderStageInfo[i].stage = VK_SHADER_STAGE_COMPUTE_BIT;

		computePipelineCreateInfo[i].sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		computePipelineCreateInfo[i].layout = playout;
		computePipelineCreateInfo[i].stage = shaderStageInfo[0];
	}

	if (vkCreateComputePipelines(
	            mvkobjs.vkdevice.device,
	            VK_NULL_HANDLE,
	            shaderStageInfo.size(),
	            computePipelineCreateInfo.data(),
	            nullptr,
	            &pipeline) != VK_SUCCESS) {
		return false;
	}

	for (size_t i{0}; i < shaders.size(); i++)
		vkDestroyShaderModule(mvkobjs.vkdevice.device, shaders[i], nullptr);

	return true;
}

inline void cleanup(rvkbucket &mvkobjs, VkPipeline &pipeline) {
	vkDestroyPipeline(mvkobjs.vkdevice.device, pipeline, nullptr);
}

};
