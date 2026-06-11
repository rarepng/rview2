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

inline VkPipeline gpline{};
inline VkPipeline cpline{};
inline uint32_t bindless_idx = 0;

inline std::vector<P> Ps(4096);
inline std::vector<std::pair<VkBuffer, VmaAllocation>> ssbobuffsnallocs(1);

inline bool createeverything(rvkbucket &objs) {

	std::default_random_engine randomengine((unsigned)time(nullptr));
	std::uniform_real_distribution<float> randomfloatdistrib(0.0f, 1.0f);

	for (auto &p : Ps) {
		float r = 0.25 * sqrt(randomfloatdistrib(randomengine));
		float theta = randomfloatdistrib(randomengine) * 2.0f * 3.14f;
		float x = r * cos(theta) * 600 / 900;
		float y = r * sin(theta);
		p.position = glm::vec2(x, y);
		p.velocity = glm::normalize(glm::vec2(x, y)) * 0.00025f;
		p.color = glm::vec4(randomfloatdistrib(randomengine), randomfloatdistrib(randomengine),
		                    randomfloatdistrib(randomengine), 1.0f);
	}

	size_t stagingbuffersize{sizeof(P)* Ps.size()};
	VkBuffer stagingbuffer = VK_NULL_HANDLE;
	VmaAllocation stagingbufferalloc = nullptr;

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = stagingbuffersize;
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	VmaAllocationCreateInfo vmaAllocInfo{};
	vmaAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
	vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

	if (vmaCreateBuffer(objs.alloc, &bufferInfo, &vmaAllocInfo, &stagingbuffer, &stagingbufferalloc, nullptr) != VK_SUCCESS) {
		return false;
	}

	void* data;
	vmaMapMemory(objs.alloc, stagingbufferalloc, &data);
	memcpy(data, Ps.data(), stagingbuffersize);
	vmaUnmapMemory(objs.alloc, stagingbufferalloc);

	for (auto &[x, y] : ssbobuffsnallocs) {
		VkBufferCreateInfo bufferInfo2{};
		bufferInfo2.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo2.size = stagingbuffersize;
		bufferInfo2.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

		VmaAllocationCreateInfo vmaAllocInfo2{};
		vmaAllocInfo2.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

		if (vmaCreateBuffer(objs.alloc, &bufferInfo2, &vmaAllocInfo2, &x, &y, nullptr) != VK_SUCCESS) {
			return false;
		}

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = objs.cpools_graphics.at(0);
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer cbuffer;

		if (vkAllocateCommandBuffers(objs.vkdevice.device, &allocInfo, &cbuffer) != VK_SUCCESS) {
			return false;
		}

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(cbuffer, &beginInfo);

		VkBufferCopy2 copyRegion2 = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
			.srcOffset = 0,
			.dstOffset = 0,
			.size = stagingbuffersize,
		};
		VkCopyBufferInfo2 copyInfo = {
			.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
			.srcBuffer = stagingbuffer,
			.dstBuffer = x,
			.regionCount = 1,
			.pRegions = &copyRegion2,
		};
		vkCmdCopyBuffer2(cbuffer, &copyInfo);
		vkEndCommandBuffer(cbuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cbuffer;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		VkFence tempFence;

		if (vkCreateFence(objs.vkdevice.device, &fenceInfo, nullptr, &tempFence) != VK_SUCCESS) {
			return false;
		}

		rview::core::mtx2->lock();

		if (vkQueueSubmit(objs.graphicsQ, 1, &submitInfo, tempFence) != VK_SUCCESS) {
			rview::core::mtx2->unlock();
			return false;
		}

		rview::core::mtx2->unlock();

		vkWaitForFences(objs.vkdevice.device, 1, &tempFence, VK_TRUE, UINT64_MAX);
		vkDestroyFence(objs.vkdevice.device, tempFence, nullptr);
		vkFreeCommandBuffers(objs.vkdevice.device, objs.cpools_graphics.at(0), 1, &cbuffer);
	}

	vmaDestroyBuffer(objs.alloc, stagingbuffer, stagingbufferalloc);

	VkShaderModule p = vkshader::loadshader(objs.vkdevice.device, "shaders/ppx.spv");
	VkShaderModule c = vkshader::loadshader(objs.vkdevice.device, "shaders/pcx.spv");
	VkShaderModule v = vkshader::loadshader(objs.vkdevice.device, "shaders/pvx.spv");

	bindless_idx = rview::core::globalBufferCounter.fetch_add(1, std::memory_order_relaxed);

	VkDescriptorBufferInfo binfo{ ssbobuffsnallocs.at(0).first, 0, VK_WHOLE_SIZE };
	VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	write.dstSet = rview::core::globalBindlessSet;
	write.dstBinding = 10;
	write.dstArrayElement = bindless_idx;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	write.descriptorCount = 1;
	write.pBufferInfo = &binfo;

	{
		std::lock_guard<std::shared_mutex> lock(*rview::core::mtx2);
		vkUpdateDescriptorSets(objs.vkdevice.device, 1, &write, 0, nullptr);
	}

	VkPipelineShaderStageCreateInfo cshaderinfo{};
	cshaderinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	cshaderinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	cshaderinfo.module = c;
	cshaderinfo.pName = "main";

	VkComputePipelineCreateInfo cplineinfo{};
	cplineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	cplineinfo.layout = rview::core::globalPipelineLayout;
	cplineinfo.stage = cshaderinfo;

	if (vkCreateComputePipelines(objs.vkdevice.device, VK_NULL_HANDLE, 1, &cplineinfo, VK_NULL_HANDLE, &cpline) != VK_SUCCESS) {
		return false;
	}

	std::array<VkPipelineShaderStageCreateInfo, 2> gshaderinfos{};
	gshaderinfos.at(0).sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	gshaderinfos.at(0).stage = VK_SHADER_STAGE_VERTEX_BIT;
	gshaderinfos.at(0).module = v;
	gshaderinfos.at(0).pName = "main";
	gshaderinfos.at(1).sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	gshaderinfos.at(1).stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	gshaderinfos.at(1).module = p;
	gshaderinfos.at(1).pName = "main";

	VkPipelineVertexInputStateCreateInfo vinputinfo{};
	vinputinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vinputinfo.vertexBindingDescriptionCount = 0;
	vinputinfo.vertexAttributeDescriptionCount = 0;

	VkPipelineInputAssemblyStateCreateInfo inputass{};
	inputass.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputass.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
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
	colorattach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorattach.blendEnable = VK_TRUE;
	colorattach.colorBlendOp = VK_BLEND_OP_ADD;
	colorattach.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorattach.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorattach.alphaBlendOp = VK_BLEND_OP_ADD;
	colorattach.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorattach.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

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
	plineinfo.layout = rview::core::globalPipelineLayout;
	plineinfo.renderPass = VK_NULL_HANDLE;
	plineinfo.subpass = 0;
	plineinfo.basePipelineHandle = VK_NULL_HANDLE;
	plineinfo.pNext = &plinerenderinginfo;

	if (vkCreateGraphicsPipelines(objs.vkdevice.device, VK_NULL_HANDLE, 1, &plineinfo, nullptr, &gpline) != VK_SUCCESS) {
		return false;
	}

	vkDestroyShaderModule(objs.vkdevice.device, c, VK_NULL_HANDLE);
	vkDestroyShaderModule(objs.vkdevice.device, v, VK_NULL_HANDLE);
	vkDestroyShaderModule(objs.vkdevice.device, p, VK_NULL_HANDLE);

	return true;
}

static inline bool drawcomp(rvkbucket &objs) {

	if (vkResetCommandBuffer(objs.cbuffers_compute.at(0).at(rview::core::currentFrame), 0) != VK_SUCCESS)
		return false;

	VkCommandBufferBeginInfo cmdbgninfo{};
	cmdbgninfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	if (vkBeginCommandBuffer(objs.cbuffers_compute.at(0).at(rview::core::currentFrame), &cmdbgninfo) != VK_SUCCESS)
		return false;

	vkCmdBindPipeline(objs.cbuffers_compute.at(0).at(rview::core::currentFrame), VK_PIPELINE_BIND_POINT_COMPUTE, cpline);

	vkCmdBindDescriptorSets(objs.cbuffers_compute.at(0).at(rview::core::currentFrame),
	                        VK_PIPELINE_BIND_POINT_COMPUTE,
	                        rview::core::globalPipelineLayout,
	                        0, 1, &rview::core::globalBindlessSet, 0, nullptr);

	vkpushconstants pc{};
	pc.posIdx = bindless_idx;
	vkCmdPushConstants(objs.cbuffers_compute.at(0).at(rview::core::currentFrame),
	                   rview::core::globalPipelineLayout,
	                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
	                   0, sizeof(vkpushconstants), &pc);

	vkCmdDispatch(objs.cbuffers_compute.at(0).at(rview::core::currentFrame), Ps.size() / 256, 1, 1);

	if (vkEndCommandBuffer(objs.cbuffers_compute.at(0).at(rview::core::currentFrame)) != VK_SUCCESS)
		return false;

	VkSubmitInfo submitinfo{};
	submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitinfo.commandBufferCount = 1;
	submitinfo.pCommandBuffers = &objs.cbuffers_compute.at(0).at(rview::core::currentFrame);
	submitinfo.signalSemaphoreCount = 1;
	submitinfo.pSignalSemaphores = &objs.semaphorez.at(2).at(rview::core::currentFrame);

	rview::core::mtx2->lock();

	if (vkQueueSubmit(objs.computeQ, 1, &submitinfo, VK_NULL_HANDLE) != VK_SUCCESS) {
		rview::core::mtx2->unlock();
		return false;
	}

	rview::core::mtx2->unlock();

	return true;
}

static inline void record_compute(VkCommandBuffer c) {
	vkCmdBindPipeline(c, VK_PIPELINE_BIND_POINT_COMPUTE, cpline);

	vkCmdBindDescriptorSets(c, VK_PIPELINE_BIND_POINT_COMPUTE,
	                        rview::core::globalPipelineLayout,
	                        0, 1, &rview::core::globalBindlessSet, 0, VK_NULL_HANDLE);

	vkpushconstants pc{};
	pc.posIdx = bindless_idx;
	vkCmdPushConstants(c, rview::core::globalPipelineLayout,
	                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
	                   0, sizeof(vkpushconstants), &pc);

	vkCmdDispatch(c, Ps.size() / 256, 1, 1);
}

static inline void destroyeveryting(rvkbucket &objs) {
	vkDestroyPipeline(objs.vkdevice.device, gpline, VK_NULL_HANDLE);
	vkDestroyPipeline(objs.vkdevice.device, cpline, VK_NULL_HANDLE);
	vmaDestroyBuffer(objs.alloc, ssbobuffsnallocs.at(0).first, ssbobuffsnallocs.at(0).second);
}

}