#include "ssbomesh.hpp"

#include <VkBootstrap.h>

bool ssbomesh::init(vkobjs &objs, ssbodata &SSBOData) {
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = 2; //////////////////////////////////////////////////////fix
	bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	VmaAllocationCreateInfo vmaAllocInfo{};
	vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

	if (vmaCreateBuffer(objs.alloc, &bufferInfo, &vmaAllocInfo, &SSBOData.buffer, &SSBOData.alloc,
	                    nullptr) != VK_SUCCESS) {
		return false;
	}

	VkDescriptorSetLayoutBinding ssboBind{};
	ssboBind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	ssboBind.binding = 0;
	ssboBind.descriptorCount = 1;
	ssboBind.pImmutableSamplers = nullptr;
	ssboBind.stageFlags = VK_SHADER_STAGE_MESH_BIT_EXT;

	VkDescriptorSetLayoutCreateInfo ssboCreateInfo{};
	ssboCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	ssboCreateInfo.bindingCount = 1;
	ssboCreateInfo.pBindings = &ssboBind;

	if (vkCreateDescriptorSetLayout(objs.vkdevice.device, &ssboCreateInfo, nullptr,
	                                &SSBOData.dlayout) != VK_SUCCESS) {
		return false;
	}

	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSize.descriptorCount = 1;

	VkDescriptorPoolCreateInfo descriptorPool{};
	descriptorPool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPool.poolSizeCount = 1;
	descriptorPool.pPoolSizes = &poolSize;
	descriptorPool.maxSets = 1;

	if (vkCreateDescriptorPool(objs.vkdevice.device, &descriptorPool, nullptr, &SSBOData.dpool) !=
	        VK_SUCCESS) {
		return false;
	}

	VkDescriptorSetAllocateInfo descriptorAllocateInfo{};
	descriptorAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorAllocateInfo.descriptorPool = SSBOData.dpool;
	descriptorAllocateInfo.descriptorSetCount = 1;
	descriptorAllocateInfo.pSetLayouts = &SSBOData.dlayout;

	if (vkAllocateDescriptorSets(objs.vkdevice.device, &descriptorAllocateInfo, &SSBOData.dset) !=
	        VK_SUCCESS) {
		return false;
	}

	VkDescriptorBufferInfo ssboInfo{};
	ssboInfo.buffer = SSBOData.buffer;
	ssboInfo.offset = 0;
	ssboInfo.range = 2; //////////////////////////////////////////////////////////////fix

	VkWriteDescriptorSet writeDescriptorSet{};
	writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writeDescriptorSet.dstSet = SSBOData.dset;
	writeDescriptorSet.dstBinding = 0;
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.pBufferInfo = &ssboInfo;

	vkUpdateDescriptorSets(objs.vkdevice.device, 1, &writeDescriptorSet, 0, nullptr);

	SSBOData.size = 2; /////////////////////////////////////////////////////fix
	return true;
}

void ssbomesh::upload(vkobjs &objs, ssbodata &ssbodata, std::vector<glm::mat4> &mats) {
	if (mats.size() == 0) {
		return;
	}

	void *data;
	vmaMapMemory(objs.alloc, ssbodata.alloc, &data);
	std::memcpy(data, mats.data(), ssbodata.size);
	vmaUnmapMemory(objs.alloc, ssbodata.alloc);
}

void ssbomesh::upload(vkobjs &objs, ssbodata &ssbodata, std::vector<glm::mat2x4> mats) {
	if (mats.size() == 0) {
		return;
	}

	void *data;
	vmaMapMemory(objs.alloc, ssbodata.alloc, &data);
	std::memcpy(data, mats.data(), ssbodata.size);
	vmaUnmapMemory(objs.alloc, ssbodata.alloc);
}

void ssbomesh::cleanup(vkobjs &objs, ssbodata &ssbodata) {
	vkDestroyDescriptorPool(objs.vkdevice.device, ssbodata.dpool, nullptr);
	vkDestroyDescriptorSetLayout(objs.vkdevice.device, ssbodata.dlayout, nullptr);
	vmaDestroyBuffer(objs.alloc, ssbodata.buffer, ssbodata.alloc);
}
