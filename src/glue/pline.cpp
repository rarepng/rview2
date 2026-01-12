#include <vector>

#include "pline.hpp"
#include "vkshader.hpp"

#include <VkBootstrap.h>
#include <glm/glm.hpp>

bool pline::init(rvkbucket &objs, VkPipelineLayout &playout, VkPipeline &pipeline, VkPrimitiveTopology topology,
                 unsigned int v_in, unsigned int atts, std::vector<std::string> sfiles, bool char_or_short, VkFormat cformat, VkFormat dformat) {
	if (sfiles.size() < 2)
		return false;
	std::vector<VkShaderModule> shaders;
	std::vector<VkPipelineShaderStageCreateInfo> shaderStageInfo;
	shaders.reserve(sfiles.size());
	shaders.resize(sfiles.size());
	shaderStageInfo.reserve(sfiles.size());
	shaderStageInfo.resize(sfiles.size());

	for (size_t i{0}; i < sfiles.size(); i++) {
		shaders[i] = vkshader::loadshader(objs.vkdevice.device, sfiles[i]);
		shaderStageInfo[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageInfo[i].module = shaders[i];
		shaderStageInfo[i].pName = "main";
	}
	shaderStageInfo.front().stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStageInfo.back().stage = VK_SHADER_STAGE_FRAGMENT_BIT;

	// vxinput as of now
	// loc 0: pos
	// loc 1: nor
	// loc 2: tan
	// loc 3: uv
	// loc 4: joints
	// loc 5: weights

	std::vector<VkVertexInputBindingDescription> bindings;
	std::vector<VkVertexInputAttributeDescription> attribs;

	auto add = [&](uint32_t size, VkFormat fmt) {
		uint32_t idx = static_cast<uint32_t>(bindings.size());
		bindings.push_back({idx, size, VK_VERTEX_INPUT_RATE_VERTEX});
		attribs.push_back({idx, idx, fmt, 0});
	};

	add(sizeof(glm::vec3), VK_FORMAT_R32G32B32_SFLOAT);
	add(sizeof(glm::vec3), VK_FORMAT_R32G32B32_SFLOAT);
	add(sizeof(glm::vec4), VK_FORMAT_R32G32B32A32_SFLOAT);
	add(sizeof(glm::vec2), VK_FORMAT_R32G32_SFLOAT);

	//starting to reconsider this decision
	if (!char_or_short) add(sizeof(uint8_t)*4, VK_FORMAT_R8G8B8A8_UINT);
	else add(sizeof(uint32_t)*4, VK_FORMAT_R32G32B32A32_UINT);

	add(sizeof(glm::vec4), VK_FORMAT_R32G32B32A32_SFLOAT);

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
	vertexInputInfo.vertexBindingDescriptionCount = (uint32_t)bindings.size();
	vertexInputInfo.pVertexBindingDescriptions = bindings.data();
	vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)attribs.size();
	vertexInputInfo.pVertexAttributeDescriptions = attribs.data();

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
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
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

	if (vkCreateGraphicsPipelines(objs.vkdevice.device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline) != VK_SUCCESS) {
		vkDestroyPipelineLayout(objs.vkdevice.device, playout, nullptr);
		return false;
	}

	for (const auto &s : shaders) {
		vkDestroyShaderModule(objs.vkdevice.device, s, nullptr);
	}

	return true;
}

void pline::cleanup(rvkbucket &objs, VkPipeline &pipeline) {
	vkDestroyPipeline(objs.vkdevice.device, pipeline, nullptr);
}
