#include "ubo.hpp"
#include <VkBootstrap.h>

bool ubo::init(rvkbucket &mvkobjs, std::span<ubodata> ubodata) {

	for (size_t i{0}; i < ubodata.size(); i++) {
		VkBufferCreateInfo binfo{};
		binfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		binfo.size = (2 * sizeof(glm::mat4)) + sizeof(glm::vec3) + sizeof(unsigned int);
		binfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

		VmaAllocationCreateInfo vmaallocinfo{};
		vmaallocinfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

		if (vmaCreateBuffer(mvkobjs.alloc, &binfo, &vmaallocinfo, &ubodata[i].buffer,
		                    &ubodata[i].alloc, nullptr) != VK_SUCCESS)
			return false;
	}


	VkDescriptorSetAllocateInfo dallocinfo{};
	dallocinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	dallocinfo.descriptorPool = mvkobjs.dpools[rvkbucket::idxinitpool];
	dallocinfo.descriptorSetCount = 1;
	dallocinfo.pSetLayouts = &rvkbucket::ubolayout;

	if (vkAllocateDescriptorSets(mvkobjs.vkdevice.device, &dallocinfo, &ubodata[0].dset) != VK_SUCCESS)
		return false;

	std::vector<VkDescriptorBufferInfo> uinfo(1);
	uinfo[0].buffer = ubodata[0].buffer;
	uinfo[0].offset = 0;
	uinfo[0].range = (2 * sizeof(glm::mat4)) + sizeof(glm::vec3) + sizeof(unsigned int);

	for (size_t i{0}; i < ubodata.size(); i++) {
		VkWriteDescriptorSet writedset{};
		writedset.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writedset.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writedset.dstSet = ubodata[0].dset;
		writedset.dstBinding = i;
		writedset.descriptorCount = 1;
		writedset.pBufferInfo = &uinfo.at(i);

		vkUpdateDescriptorSets(mvkobjs.vkdevice.device, 1, &writedset, 0, nullptr);

		ubodata[0].size = (2 * sizeof(glm::mat4)) + sizeof(glm::vec3) + sizeof(unsigned int);
	}

	return true;
}
bool ubo::createlayout(rvkbucket &mvkobjs,VkDescriptorSetLayout& dlayout) {
	std::vector<VkDescriptorSetLayoutBinding> ubobind(1);
	ubobind[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ubobind[0].binding = 0;
	ubobind[0].descriptorCount = 1;
	ubobind[0].pImmutableSamplers = nullptr;
	ubobind[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;


	VkDescriptorSetLayoutCreateInfo uboinfo{};
	uboinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	uboinfo.bindingCount = 1;
	uboinfo.pBindings = ubobind.data();

	if (vkCreateDescriptorSetLayout(mvkobjs.vkdevice.device, &uboinfo, nullptr, &dlayout) !=
	        VK_SUCCESS)
		return false;
	return true;
}
void ubo::upload(rvkbucket &mvkobjs, std::vector<ubodata> &ubodata, std::vector<glm::mat4> mats,const glm::vec3& campos) {
	//todo: fix this mess
	glm::vec4 campos4(campos, 0.0f);
	void* data;
	vmaMapMemory(mvkobjs.alloc, ubodata[0].alloc, &data);
	std::byte* dst = static_cast<std::byte*>(data);
	std::memcpy(dst, mats.data(), mats.size() * sizeof(glm::mat4));
	std::memcpy(dst+(mats.size() * sizeof(glm::mat4)), &campos4, sizeof(campos4));
	vmaUnmapMemory(mvkobjs.alloc, ubodata[0].alloc);
}

void ubo::cleanup(rvkbucket &mvkobjs, std::vector<ubodata> &ubodata) {
	for (size_t i{0}; i < ubodata.size(); i++) {
		vmaDestroyBuffer(mvkobjs.alloc, ubodata[i].buffer, ubodata[i].alloc);
	}
}
