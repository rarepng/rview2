#include "ubo.hpp"
#include <VkBootstrap.h>

bool ubo::init(rvk &mvkobjs, std::vector<ubodata> &ubodata) {
	ubodata.reserve(2);
	ubodata.resize(2);

	for (size_t i{0}; i < ubodata.size(); i++) {
		VkBufferCreateInfo binfo{};
		binfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		if (i < 1)
			binfo.size = 2 * sizeof(glm::mat4);
		else
			binfo.size = sizeof(unsigned int);
		binfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

		VmaAllocationCreateInfo vmaallocinfo{};
		vmaallocinfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

		if (vmaCreateBuffer(mvkobjs.alloc, &binfo, &vmaallocinfo, &ubodata[i].buffer,
		                    &ubodata[i].alloc, nullptr) != VK_SUCCESS)
			return false;
	}


	VkDescriptorSetAllocateInfo dallocinfo{};
	dallocinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	dallocinfo.descriptorPool = mvkobjs.dpools[rvk::idxinitpool];
	dallocinfo.descriptorSetCount = 1;
	dallocinfo.pSetLayouts = &rvk::ubolayout;

	if (vkAllocateDescriptorSets(mvkobjs.vkdevice.device, &dallocinfo, &ubodata[0].dset) != VK_SUCCESS)
		return false;

	std::vector<VkDescriptorBufferInfo> uinfo(2);
	uinfo[0].buffer = ubodata[0].buffer;
	uinfo[0].offset = 0;
	uinfo[0].range = 2 * sizeof(glm::mat4);
	uinfo[1].buffer = ubodata[1].buffer;
	uinfo[1].offset = 0;
	uinfo[1].range = sizeof(unsigned int);

	for (size_t i{0}; i < ubodata.size(); i++) {
		VkWriteDescriptorSet writedset{};
		writedset.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writedset.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writedset.dstSet = ubodata[0].dset;
		writedset.dstBinding = i;
		writedset.descriptorCount = 1;
		writedset.pBufferInfo = &uinfo.at(i);

		vkUpdateDescriptorSets(mvkobjs.vkdevice.device, 1, &writedset, 0, nullptr);

		if (i < 1)
			ubodata[i].size = 2 * sizeof(glm::mat4);
		else
			ubodata[i].size = sizeof(unsigned int);
	}

	return true;
}
bool ubo::createlayout(rvk &mvkobjs,VkDescriptorSetLayout& dlayout) {
	std::vector<VkDescriptorSetLayoutBinding> ubobind(2);
	ubobind[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ubobind[0].binding = 0;
	ubobind[0].descriptorCount = 1;
	ubobind[0].pImmutableSamplers = nullptr;
	ubobind[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	ubobind[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ubobind[1].binding = 1;
	ubobind[1].descriptorCount = 1;
	ubobind[1].pImmutableSamplers = nullptr;
	ubobind[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;


	VkDescriptorSetLayoutCreateInfo uboinfo{};
	uboinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	uboinfo.bindingCount = 2;
	uboinfo.pBindings = ubobind.data();

	if (vkCreateDescriptorSetLayout(mvkobjs.vkdevice.device, &uboinfo, nullptr, &dlayout) !=
	        VK_SUCCESS)
		return false;
	return true;
}
void ubo::upload(rvk &mvkobjs, std::vector<ubodata> &ubodata, std::vector<glm::mat4> mats) {
	void *data;
	vmaMapMemory(mvkobjs.alloc, ubodata[0].alloc, &data);
	std::memcpy(data, mats.data(), ubodata[0].size);
	vmaUnmapMemory(mvkobjs.alloc, ubodata[0].alloc);
}

void ubo::cleanup(rvk &mvkobjs, std::vector<ubodata> &ubodata) {
	for (size_t i{0}; i < ubodata.size(); i++) {
		vmaDestroyBuffer(mvkobjs.alloc, ubodata[i].buffer, ubodata[i].alloc);
	}
}
