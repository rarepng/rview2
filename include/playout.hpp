#pragma once
#include "core/rvk.hpp"
#include <vulkan/vulkan.h>
#include "VkBootstrap.h"
#include <pline.hpp>


namespace playout {
inline VkPipeline skinnedpline = VK_NULL_HANDLE;
inline VkPipeline skinnedplineuint = VK_NULL_HANDLE;

inline bool init_pipelines(rvkbucket& objs) {
	// hardcoded shaders
	if (!pline::init(objs, rview::core::globalPipelineLayout, skinnedpline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	                 std::vector<std::string_view> {"shaders/vx.spv", "shaders/px.spv"}, objs.schain.image_format, objs.rddepthformat))
		return false;

	if (!pline::init(objs, rview::core::globalPipelineLayout, skinnedplineuint, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	                 std::vector<std::string_view> {"shaders/vx.spv", "shaders/px.spv"}, objs.schain.image_format, objs.rddepthformat))
		return false;

	if (!pline::initcompute(objs, rview::core::globalPipelineLayout, rview::core::globalcullpline, std::vector<std::string_view> {"shaders/cx.spv"}))
		return false;

	return true;
}
inline void cleanup_pipelines(rvkbucket& objs) {
	pline::cleanup(objs, skinnedpline);
	pline::cleanup(objs, skinnedplineuint);
	pline::cleanup(objs, rview::core::globalcullpline);
}
inline bool init_bindless(rvkbucket& objs) {
	constexpr uint32_t MAX_BINDLESS = 32768;

	for (uint32_t i = 1; i < rview::core::MAX_BINDLESS_BUFFERS; ++i) {
		rview::core::global_buffers.free_slots.push(i);
	}

	std::array<VkDescriptorSetLayoutBinding, 19> bindings{};

	// tex
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[0].descriptorCount = MAX_BINDLESS;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;

	// ubo (cam)
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	// ssbo (joints)
	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[2].descriptorCount = rview::core::MAX_FRAMES_IN_FLIGHT;
	bindings[2].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	// env
	bindings[3].binding = 3;
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[3].descriptorCount = 1;
	bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// ssbo (mats)
	bindings[4].binding = 4;
	bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[4].descriptorCount = 1;
	bindings[4].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	// vbo
	bindings[5].binding = 5;
	bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[5].descriptorCount = MAX_BINDLESS;
	bindings[5].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	// ebo
	bindings[6].binding = 6;
	bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[6].descriptorCount = MAX_BINDLESS;
	bindings[6].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	// instances [[culling]]
	bindings[7].binding = 7;
	bindings[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[7].descriptorCount = rview::core::MAX_FRAMES_IN_FLIGHT;
	bindings[7].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

	// indirect drawing
	bindings[8].binding = 8;
	bindings[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[8].descriptorCount = rview::core::MAX_FRAMES_IN_FLIGHT;
	bindings[8].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	// visibility buffer?
	bindings[9].binding = 9;
	bindings[9].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[9].descriptorCount = rview::core::MAX_FRAMES_IN_FLIGHT;
	bindings[9].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	// particle system(s)
	bindings[10].binding = 10;
	bindings[10].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[10].descriptorCount = MAX_BINDLESS;
	bindings[10].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

	// 12th [[deferred]]
	bindings[11].binding = 11;
	bindings[11].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[11].descriptorCount = rview::core::MAX_FRAMES_IN_FLIGHT;
	bindings[11].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	// raw byte index
	bindings[12].binding = 12;
	bindings[12].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[12].descriptorCount = 1;
	bindings[12].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

	// Binding 13: Primitive Registry
	bindings[13].binding = 13;
	bindings[13].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[13].descriptorCount = 1;
	bindings[13].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

	// Binding 14: Model Registry
	bindings[14].binding = 14;
	bindings[14].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[14].descriptorCount = 1;
	bindings[14].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	// indirect UNindexed commands
	bindings[15].binding = 15;
	bindings[15].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[15].descriptorCount = rview::core::MAX_FRAMES_IN_FLIGHT;
	bindings[15].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	// indirect count
	bindings[16].binding = 16;
	bindings[16].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[16].descriptorCount = rview::core::MAX_FRAMES_IN_FLIGHT;
	bindings[16].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	// static morph deltas
	bindings[17].binding = 17;
	bindings[17].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[17].descriptorCount = 1;
	bindings[17].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	// dynamic morph targets per frame
	bindings[18].binding = 18;
	bindings[18].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[18].descriptorCount = rview::core::MAX_FRAMES_IN_FLIGHT;
	bindings[18].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	std::array<VkDescriptorBindingFlags, 19> flags{};
	VkDescriptorBindingFlags bindlessFlags =
	    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
	    VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

	flags[0] = bindlessFlags;
	flags[1] = 0;
	flags[2] = bindlessFlags;
	flags[3] = 0;
	flags[4] = 0;
	flags[5] = bindlessFlags;
	flags[6] = bindlessFlags;
	flags[7] = bindlessFlags;
	flags[8] = bindlessFlags;
	flags[9] = bindlessFlags;
	flags[10] = bindlessFlags;
	flags[11] = bindlessFlags;
	flags[12] = bindlessFlags;
	flags[13] = bindlessFlags;
	flags[14] = bindlessFlags;
	flags[15] = bindlessFlags;
	flags[16] = bindlessFlags;
	flags[17] = bindlessFlags;
	flags[18] = bindlessFlags;

	VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
	flagsInfo.bindingCount = static_cast<uint32_t>(flags.size());
	flagsInfo.pBindingFlags = flags.data();

	VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
	layoutInfo.pNext = &flagsInfo;
	layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(objs.vkdevice.device, &layoutInfo, nullptr, &rview::core::globalBindlessLayout) != VK_SUCCESS)
		return false;

	std::array<VkDescriptorPoolSize, 3> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[0].descriptorCount = MAX_BINDLESS + 2;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[1].descriptorCount = 1;
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSizes[2].descriptorCount = (MAX_BINDLESS * 3) + (rview::core::MAX_FRAMES_IN_FLIGHT * 9) + 6; // total descriptor counts

	VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
	poolInfo.maxSets = 1;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();

	if (vkCreateDescriptorPool(objs.vkdevice.device, &poolInfo, nullptr, &rview::core::globalBindlessPool) != VK_SUCCESS)
		return false;

	VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
	allocInfo.descriptorPool = rview::core::globalBindlessPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &rview::core::globalBindlessLayout;

	if (vkAllocateDescriptorSets(objs.vkdevice.device, &allocInfo, &rview::core::globalBindlessSet) != VK_SUCCESS)
		return false;

	VkPushConstantRange pCs{};
	pCs.offset = 0;
	pCs.size = sizeof(vkpushconstants);
	pCs.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

	VkPipelineLayoutCreateInfo plinfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	plinfo.setLayoutCount = 1;
	plinfo.pSetLayouts = &rview::core::globalBindlessLayout;
	plinfo.pushConstantRangeCount = 1;
	plinfo.pPushConstantRanges = &pCs;

	if (vkCreatePipelineLayout(objs.vkdevice.device, &plinfo, nullptr, &rview::core::globalPipelineLayout) != VK_SUCCESS) {
		return false;
	}

	return true;
}
inline void update_asset_descriptors(rvkbucket& mvkobjs) {
	std::vector<VkWriteDescriptorSet> writes;
	std::array<VkDescriptorBufferInfo, 4> bInfos{};

	uint32_t writeCount = 0;

	if (rview::core::g_rawIndexSSBO.buffer != VK_NULL_HANDLE) {
		bInfos[writeCount].buffer = rview::core::g_rawIndexSSBO.buffer;
		bInfos[writeCount].offset = 0;
		bInfos[writeCount].range = VK_WHOLE_SIZE;

		VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		write.dstSet = rview::core::globalBindlessSet;
		write.dstBinding = 12;
		write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		write.descriptorCount = 1;
		write.pBufferInfo = &bInfos[writeCount];

		writes.push_back(write);
		writeCount++;
	}

	if (rview::core::g_primitiveRegistrySSBO.buffer != VK_NULL_HANDLE) {
		bInfos[writeCount].buffer = rview::core::g_primitiveRegistrySSBO.buffer;
		bInfos[writeCount].offset = 0;
		bInfos[writeCount].range = VK_WHOLE_SIZE;

		VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		write.dstSet = rview::core::globalBindlessSet;
		write.dstBinding = 13;
		write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		write.descriptorCount = 1;
		write.pBufferInfo = &bInfos[writeCount];

		writes.push_back(write);
		writeCount++;
	}

	if (rview::core::g_modelRegistrySSBO.buffer != VK_NULL_HANDLE) {
		bInfos[writeCount].buffer = rview::core::g_modelRegistrySSBO.buffer;
		bInfos[writeCount].offset = 0;
		bInfos[writeCount].range = VK_WHOLE_SIZE;

		VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		write.dstSet = rview::core::globalBindlessSet;
		write.dstBinding = 14;
		write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		write.descriptorCount = 1;
		write.pBufferInfo = &bInfos[writeCount];

		writes.push_back(write);
		writeCount++;
	}

	if (g_morphDeltaSSBO.buffer != VK_NULL_HANDLE) {
		bInfos[writeCount].buffer = g_morphDeltaSSBO.buffer;
		bInfos[writeCount].offset = 0;
		bInfos[writeCount].range = VK_WHOLE_SIZE;

		VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		write.dstSet = rview::core::globalBindlessSet;
		write.dstBinding = 17;
		write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		write.descriptorCount = 1;
		write.pBufferInfo = &bInfos[writeCount];

		writes.push_back(write);
		writeCount++;
	}

	if (!writes.empty()) {
		std::lock_guard<std::shared_mutex> lock(*rview::core::mtx2);
		vkUpdateDescriptorSets(mvkobjs.vkdevice.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
	}
}

inline void cleanup(rvkbucket &mvkobjs, VkPipelineLayout &vkplayout) {
	vkDestroyPipelineLayout(mvkobjs.vkdevice.device, vkplayout, nullptr);
}

};