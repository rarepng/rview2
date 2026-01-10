#pragma once
#include "core/rvk.hpp"
#include "vkshader.hpp"
#include <array>
#include <random>

struct P {
	glm::vec2 position;
	glm::vec2 velocity;
	glm::vec4 color;
};
namespace particle {

static VkPipeline gpline;
static VkPipeline cpline{};
static VkPipelineLayout cplayout{};
static VkPipelineLayout gplayout{};
static VkDescriptorSetLayout cdlayout{};
static VkDescriptorSet cdset{};

static std::vector<P> Ps(8128);




static std::vector<std::pair<VkBuffer, VmaAllocation>> ssbobuffsnallocs(1);


static VkDescriptorPool dpool{VK_NULL_HANDLE};

static inline bool createeverything(rvk &objs) {

	// random
	std::default_random_engine randomengine((unsigned)time(nullptr));
	std::uniform_real_distribution<float> randomfloatdistrib(0.0f, 1.0f);

	// particles
	for (auto &p : Ps) {
		float r = 0.25 * sqrt(randomfloatdistrib(randomengine));
		float theta = randomfloatdistrib(randomengine) * 2.0f * 3.14f;
		float x = r * cos(theta) * 600 / 900;
		float y = r * sin(theta);
		p.position = glm::vec2(x, y);
		p.velocity = glm::normalize(glm::vec2(x, y)) * 0.00025f;
		p.color = glm::vec4(randomfloatdistrib(randomengine), randomfloatdistrib(randomengine),
		                    randomfloatdistrib(randomengine), 1.0f);
		// p.color = glm::vec4(0.6,0.1f,0.5f,1.0f);
		// p.velocity = glm::vec2(0.002f,0.002);
		// p.position = glm::vec2(-1.0f);
	}

	// create buffers
	size_t stagingbuffersize{sizeof(P) * Ps.size()};
	VkBuffer stagingbuffer = VK_NULL_HANDLE;
	VmaAllocation stagingbufferalloc = nullptr;

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = stagingbuffersize;
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	VmaAllocationCreateInfo vmaAllocInfo{};
	vmaAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;				       // auto
	vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT; // auto

	if (vmaCreateBuffer(objs.alloc, &bufferInfo, &vmaAllocInfo, &stagingbuffer, &stagingbufferalloc, nullptr) !=
	        VK_SUCCESS) {
		return false;
	}

	void *data;
	vmaMapMemory(objs.alloc, stagingbufferalloc, &data);
	memcpy(data, Ps.data(), stagingbuffersize);
	vmaUnmapMemory(objs.alloc, stagingbufferalloc);


	for (auto &[x, y] : ssbobuffsnallocs) {
		VkBufferCreateInfo bufferInfo2{};
		bufferInfo2.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo2.size = stagingbuffersize;
		bufferInfo2.usage =
		    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

		VmaAllocationCreateInfo vmaAllocInfo2{};
		vmaAllocInfo2.usage = VMA_MEMORY_USAGE_AUTO;				  // auto
		vmaAllocInfo2.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT; // auto

		if (vmaCreateBuffer(objs.alloc, &bufferInfo2, &vmaAllocInfo2, &x, &y, nullptr) != VK_SUCCESS) {
			return false;
		}

		VkBufferCopy2 copyRegion2 = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
			.pNext = NULL,
			.srcOffset = 0,
			.dstOffset = 0,
			.size = stagingbuffersize,
		};
		VkCopyBufferInfo2 copyInfo = {
			.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
			.pNext = NULL,
			.srcBuffer = stagingbuffer,
			.dstBuffer = x,
			.regionCount = 1,
			.pRegions = &copyRegion2,
		};

		// uploading buffer
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkResetCommandBuffer(objs.cbuffers_graphics.at(2), 0);
		vkBeginCommandBuffer(objs.cbuffers_graphics.at(2), &beginInfo);
		vkCmdCopyBuffer2(objs.cbuffers_graphics.at(2), &copyInfo);
		vkEndCommandBuffer(objs.cbuffers_graphics.at(2));

		VkSubmitInfo submitinfo{};
		submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitinfo.commandBufferCount = 1;
		submitinfo.pCommandBuffers = &objs.cbuffers_graphics.at(2);
		submitinfo.pSignalSemaphores = &objs.semaphorez.at(2).at(rvk::currentFrame);

		objs.mtx2->lock();
		if (vkQueueSubmit(objs.graphicsQ, 1, &submitinfo, VK_NULL_HANDLE) != VK_SUCCESS) {
			return false;
		}
		vkQueueWaitIdle(objs.graphicsQ);
		objs.mtx2->unlock();
	}

	vmaDestroyBuffer(objs.alloc, stagingbuffer, stagingbufferalloc);

	// shaders
	VkShaderModule p = vkshader::loadshader(objs.vkdevice.device, "shaders/ppx.spv");
	VkShaderModule c = vkshader::loadshader(objs.vkdevice.device, "shaders/pcx.spv");
	VkShaderModule v = vkshader::loadshader(objs.vkdevice.device, "shaders/pvx.spv");

	// descriptor pool
	std::vector<VkDescriptorPoolSize> pool0{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2}};
	if (!rpool::create(pool0, objs.vkdevice.device, &dpool))
		return false;

	// descriptor set layout
	std::array<VkDescriptorSetLayoutBinding, 2> dlayoutbinds{};
	dlayoutbinds.at(0).binding = 0;
	dlayoutbinds.at(0).descriptorCount = 1;
	dlayoutbinds.at(0).descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	dlayoutbinds.at(0).pImmutableSamplers = VK_NULL_HANDLE;
	dlayoutbinds.at(0).stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	dlayoutbinds.at(1).binding = 1;
	dlayoutbinds.at(1).descriptorCount = 1;
	dlayoutbinds.at(1).descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	dlayoutbinds.at(1).pImmutableSamplers = VK_NULL_HANDLE;
	dlayoutbinds.at(1).stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutCreateInfo dlayoutinfo{};
	dlayoutinfo.bindingCount = 2;
	dlayoutinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dlayoutinfo.pBindings = dlayoutbinds.data();
	if (vkCreateDescriptorSetLayout(objs.vkdevice.device, &dlayoutinfo, VK_NULL_HANDLE, &cdlayout) != VK_SUCCESS) {
		return false;
	}

	// graphics playout
	VkPipelineLayoutCreateInfo playoutinfo{};
	playoutinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	playoutinfo.setLayoutCount = 0;
	playoutinfo.pSetLayouts = VK_NULL_HANDLE;
	if (vkCreatePipelineLayout(objs.vkdevice.device, &playoutinfo, VK_NULL_HANDLE, &gplayout) != VK_SUCCESS)
		return false;

	// graphics pline
	std::array<VkPipelineShaderStageCreateInfo, 2> gshaderinfos{};
	gshaderinfos.at(0).sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	gshaderinfos.at(0).stage = VK_SHADER_STAGE_VERTEX_BIT;
	gshaderinfos.at(0).module = v;
	gshaderinfos.at(0).pName = "main";
	gshaderinfos.at(1).sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	gshaderinfos.at(1).stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	gshaderinfos.at(1).module = p;
	gshaderinfos.at(1).pName = "main";

	VkVertexInputBindingDescription inp{};
	inp.binding = 0;
	inp.stride = sizeof(P);
	inp.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	std::array<VkVertexInputAttributeDescription, 2> inpvx{};
	inpvx.at(0).binding = 0;
	inpvx.at(0).location = 0;
	inpvx.at(0).format = VK_FORMAT_R32G32_SFLOAT;
	inpvx.at(0).offset = offsetof(P, position);
	inpvx.at(1).binding = 0;
	inpvx.at(1).location = 1;
	inpvx.at(1).format = VK_FORMAT_R32G32B32A32_SFLOAT;
	inpvx.at(1).offset = offsetof(P, color);
	VkPipelineVertexInputStateCreateInfo vinputinfo{};
	vinputinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vinputinfo.vertexBindingDescriptionCount = 1;
	vinputinfo.vertexAttributeDescriptionCount = inpvx.size();
	vinputinfo.pVertexBindingDescriptions = &inp;
	vinputinfo.pVertexAttributeDescriptions = inpvx.data();

	VkPipelineInputAssemblyStateCreateInfo inputass{};
	inputass.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputass.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; ///////////////////////////////////////////////////////
	inputass.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportinfo{};
	viewportinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportinfo.viewportCount = 1;
	viewportinfo.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterinfo{};
	rasterinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterinfo.depthClampEnable = VK_FALSE;
	rasterinfo.rasterizerDiscardEnable = VK_FALSE;
	rasterinfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterinfo.lineWidth = 1.0f;
	rasterinfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterinfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterinfo.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisinfo{};
	multisinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisinfo.sampleShadingEnable = VK_FALSE;
	multisinfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colorattach{};
	colorattach.colorWriteMask =
	    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorattach.blendEnable = VK_TRUE;
	colorattach.colorBlendOp = VK_BLEND_OP_ADD;
	colorattach.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorattach.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorattach.alphaBlendOp = VK_BLEND_OP_ADD;
	colorattach.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorattach.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	VkPipelineColorBlendStateCreateInfo colorinfo{};
	colorinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorinfo.logicOpEnable = VK_FALSE;
	colorinfo.logicOp = VK_LOGIC_OP_COPY;
	colorinfo.attachmentCount = 1;
	colorinfo.pAttachments = &colorattach;
	colorinfo.blendConstants[0] = 0.0f;
	colorinfo.blendConstants[1] = 0.0f;
	colorinfo.blendConstants[2] = 0.0f;
	colorinfo.blendConstants[3] = 0.0f;
	VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};
	depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilInfo.depthTestEnable = VK_TRUE;
	depthStencilInfo.depthWriteEnable = VK_TRUE;
	depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilInfo.minDepthBounds = 0.0f;
	depthStencilInfo.maxDepthBounds = 1.0f;
	depthStencilInfo.stencilTestEnable = VK_FALSE;

	std::array<VkDynamicState, 2> dynstates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dyninfo{};
	dyninfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dyninfo.dynamicStateCount = dynstates.size();
	dyninfo.pDynamicStates = dynstates.data();


	
	VkPipelineRenderingCreateInfo plinerenderinginfo{};
	plinerenderinginfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	plinerenderinginfo.colorAttachmentCount = 1;
	plinerenderinginfo.pColorAttachmentFormats = &objs.schain.image_format;
	plinerenderinginfo.depthAttachmentFormat = objs.rddepthformat;
	plinerenderinginfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;


	VkGraphicsPipelineCreateInfo plineinfo{};
	plineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	plineinfo.stageCount = 2;
	plineinfo.pStages = gshaderinfos.data();
	plineinfo.pVertexInputState = &vinputinfo;
	plineinfo.pInputAssemblyState = &inputass;
	plineinfo.pViewportState = &viewportinfo;
	plineinfo.pRasterizationState = &rasterinfo;
	plineinfo.pMultisampleState = &multisinfo;
	plineinfo.pColorBlendState = &colorinfo;
	plineinfo.pDepthStencilState = &depthStencilInfo;
	plineinfo.pDynamicState = &dyninfo;
	plineinfo.layout = gplayout;
	plineinfo.renderPass = VK_NULL_HANDLE;
	plineinfo.subpass = 0;
	plineinfo.basePipelineHandle = VK_NULL_HANDLE;
	plineinfo.pNext = &plinerenderinginfo;

	if (vkCreateGraphicsPipelines(objs.vkdevice.device, VK_NULL_HANDLE, 1, &plineinfo, nullptr, &gpline) != VK_SUCCESS) {
		return false;
	}

	vkDestroyShaderModule(objs.vkdevice.device, p, VK_NULL_HANDLE);
	vkDestroyShaderModule(objs.vkdevice.device, v, VK_NULL_HANDLE);

	// descriptor sets
	std::array<VkDescriptorSetLayout, 1> clayouts({cdlayout});
	VkDescriptorSetAllocateInfo cdallocinfo{};
	cdallocinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	cdallocinfo.descriptorPool = dpool;
	cdallocinfo.descriptorSetCount = 1;
	cdallocinfo.pSetLayouts = clayouts.data();
	if (vkAllocateDescriptorSets(objs.vkdevice.device, &cdallocinfo, &cdset) != VK_SUCCESS) {
		return false;
	}

	VkDescriptorBufferInfo cssboinfo{}; // remake when switching to 2 frames
	cssboinfo.buffer = ssbobuffsnallocs.at(0).first;
	cssboinfo.offset = 0;
	cssboinfo.range = sizeof(P) * Ps.size();

	std::array<VkWriteDescriptorSet, 2> cdwrites{};
	cdwrites.at(0).sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	cdwrites.at(0).dstSet = cdset;
	cdwrites.at(0).dstBinding = 0;
	cdwrites.at(0).dstArrayElement = 0;
	cdwrites.at(0).descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	cdwrites.at(0).descriptorCount = 1;
	cdwrites.at(0).pBufferInfo = &cssboinfo;
	cdwrites.at(1).sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	cdwrites.at(1).dstSet = cdset;
	cdwrites.at(1).dstBinding = 1;
	cdwrites.at(1).dstArrayElement = 0;
	cdwrites.at(1).descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	cdwrites.at(1).descriptorCount = 1;
	cdwrites.at(1).pBufferInfo = &cssboinfo;

	vkUpdateDescriptorSets(objs.vkdevice.device, cdwrites.size(), cdwrites.data(), 0, VK_NULL_HANDLE);

	// compute playout
	VkPipelineLayoutCreateInfo cplayoutinfo{};
	cplayoutinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	cplayoutinfo.setLayoutCount = 1;
	cplayoutinfo.pSetLayouts = &cdlayout;
	if (vkCreatePipelineLayout(objs.vkdevice.device, &cplayoutinfo, VK_NULL_HANDLE, &cplayout) != VK_SUCCESS)
		return false;

	// compute pline
	std::array<VkPipelineShaderStageCreateInfo, 1> cshaderinfos{};
	cshaderinfos.at(0).sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	cshaderinfos.at(0).stage = VK_SHADER_STAGE_COMPUTE_BIT;
	cshaderinfos.at(0).module = c;
	cshaderinfos.at(0).pName = "main";

	VkComputePipelineCreateInfo cplineinfo{};
	cplineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	cplineinfo.layout = cplayout;
	cplineinfo.stage = cshaderinfos.at(0);

	if (vkCreateComputePipelines(objs.vkdevice.device, VK_NULL_HANDLE, 1, &cplineinfo, VK_NULL_HANDLE, &cpline) !=
	        VK_SUCCESS) {
		return false;
	}

	vkDestroyShaderModule(objs.vkdevice.device, c, VK_NULL_HANDLE);
	return true;
}

static inline bool drawcomp(rvk &objs) {

	if (vkResetCommandBuffer(objs.cbuffers_compute.at(rvk::currentFrame), 0) != VK_SUCCESS)
		return false;

	VkCommandBufferBeginInfo cmdbgninfo{};
	cmdbgninfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	if (vkBeginCommandBuffer(objs.cbuffers_compute.at(rvk::currentFrame), &cmdbgninfo) != VK_SUCCESS)
		return false;


	vkCmdBindPipeline(objs.cbuffers_compute.at(rvk::currentFrame), VK_PIPELINE_BIND_POINT_COMPUTE, cpline);

	vkCmdBindDescriptorSets(objs.cbuffers_compute.at(rvk::currentFrame),VK_PIPELINE_BIND_POINT_COMPUTE,cplayout,0,1,&cdset,0,VK_NULL_HANDLE);
	// VkBindDescriptorSetsInfo dsetinfo{};
	// dsetinfo.sType = VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_SETS_INFO;
	// dsetinfo.layout = cplayout;
	// dsetinfo.descriptorSetCount = 1;
	// dsetinfo.firstSet = 0;
	// dsetinfo.dynamicOffsetCount = 0;
	// dsetinfo.pDynamicOffsets = VK_NULL_HANDLE;
	// dsetinfo.pNext = VK_NULL_HANDLE;
	// dsetinfo.stageFlags = ;
	// dsetinfo.pDescriptorSets = &cdset;
	// vkCmdBindDescriptorSets2(objs.cbuffers_graphics.at(2), &dsetinfo);

	vkCmdDispatch(objs.cbuffers_compute.at(rvk::currentFrame), Ps.size() / 256, 1, 1);

	if (vkEndCommandBuffer(objs.cbuffers_compute.at(rvk::currentFrame)) != VK_SUCCESS)
		return false;

	VkSubmitInfo submitinfo{};
	submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitinfo.commandBufferCount = 1;
	submitinfo.pCommandBuffers = &objs.cbuffers_compute.at(rvk::currentFrame);
	submitinfo.signalSemaphoreCount = 1;
	submitinfo.pSignalSemaphores = &objs.semaphorez.at(2).at(rvk::currentFrame);

	objs.mtx2->lock();
	if (vkQueueSubmit(objs.computeQ, 1, &submitinfo, VK_NULL_HANDLE) != VK_SUCCESS) {
		return false;
	}
	objs.mtx2->unlock();

	return true;
}

static inline void destroyeveryting(rvk &objs) {
	vkDestroyPipeline(objs.vkdevice.device,gpline,VK_NULL_HANDLE);
	vkDestroyPipelineLayout(objs.vkdevice.device,gplayout,VK_NULL_HANDLE);
	vkDestroyPipeline(objs.vkdevice.device,cpline,VK_NULL_HANDLE);
	vkDestroyPipelineLayout(objs.vkdevice.device,cplayout,VK_NULL_HANDLE);

	rpool::destroy(objs.vkdevice.device,dpool);
	vkDestroyDescriptorSetLayout(objs.vkdevice.device,cdlayout,VK_NULL_HANDLE);
	vmaDestroyBuffer(objs.alloc,ssbobuffsnallocs.at(0).first,ssbobuffsnallocs.at(0).second);

}

}