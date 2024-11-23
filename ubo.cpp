#include "ubo.hpp"
#include <vk/VkBootstrap.h>


bool ubo::init(vkobjs& mvkobjs, std::vector<vkuniformbufferdata>& ubodata) {
	ubodata.reserve(2);
	ubodata.resize(2);

	for (int i{ 0 }; i < ubodata.size(); i++) {
		VkBufferCreateInfo binfo{};
		binfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		if (i < 1)binfo.size = 2 * sizeof(glm::mat4);
		else binfo.size = sizeof(unsigned int);
		binfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

		VmaAllocationCreateInfo vmaallocinfo{};
		vmaallocinfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

		if (vmaCreateBuffer(mvkobjs.rdallocator, &binfo, &vmaallocinfo, &ubodata[i].rdubobuffer, &ubodata[i].rdubobufferalloc, nullptr) != VK_SUCCESS)return false;
	}

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
	uboinfo.pBindings = &ubobind.at(0);

	if (vkCreateDescriptorSetLayout(mvkobjs.rdvkbdevice.device, &uboinfo, nullptr, &ubodata[0].rdubodescriptorlayout) != VK_SUCCESS)return false;

	VkDescriptorPoolSize poolsize{};
	poolsize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolsize.descriptorCount = 1 * (ubodata.size() * sizeof(glm::mat4) + sizeof(unsigned int));

	VkDescriptorPoolCreateInfo dpoolinfo{};
	dpoolinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	dpoolinfo.poolSizeCount = 1;
	dpoolinfo.pPoolSizes = &poolsize;
	dpoolinfo.maxSets = 1;

	if (vkCreateDescriptorPool(mvkobjs.rdvkbdevice.device, &dpoolinfo, nullptr, &ubodata[0].rdubodescriptorpool) != VK_SUCCESS)return false;

	VkDescriptorSetAllocateInfo dallocinfo{};
	dallocinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	dallocinfo.descriptorPool = ubodata[0].rdubodescriptorpool;
	dallocinfo.descriptorSetCount = 1;
	dallocinfo.pSetLayouts = &ubodata[0].rdubodescriptorlayout;

	if (vkAllocateDescriptorSets(mvkobjs.rdvkbdevice.device, &dallocinfo, &ubodata[0].rdubodescriptorset) != VK_SUCCESS)return false;

	std::vector<VkDescriptorBufferInfo> uinfo(2);
	uinfo[0].buffer = ubodata[0].rdubobuffer;
	uinfo[0].offset = 0;
	uinfo[0].range = 2 * sizeof(glm::mat4);
	uinfo[1].buffer = ubodata[1].rdubobuffer;
	uinfo[1].offset = 0;
	uinfo[1].range = sizeof(unsigned int);



	for (int i{ 0 }; i < ubodata.size(); i++) {
		VkWriteDescriptorSet writedset{};
		writedset.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writedset.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writedset.dstSet = ubodata[0].rdubodescriptorset;
		writedset.dstBinding = i;
		writedset.descriptorCount = 1;
		writedset.pBufferInfo = &uinfo.at(i);

		vkUpdateDescriptorSets(mvkobjs.rdvkbdevice.device, 1, &writedset, 0, nullptr);

		if (i < 1)ubodata[i].rduniformbuffersize = 2 * sizeof(glm::mat4);
		else ubodata[i].rduniformbuffersize = sizeof(unsigned int);

	}



	return true;
}
void ubo::upload(vkobjs& mvkobjs, std::vector<vkuniformbufferdata>& ubodata, std::vector<glm::mat4> mats,unsigned int texidx){
	void* data;
	vmaMapMemory(mvkobjs.rdallocator, ubodata[0].rdubobufferalloc, &data);
	std::memcpy(data, mats.data(), ubodata[0].rduniformbuffersize);
	vmaUnmapMemory(mvkobjs.rdallocator, ubodata[0].rdubobufferalloc);
}

void ubo::upload(vkobjs& mvkobjs, std::vector<vkuniformbufferdata>& ubodata, unsigned int texidx){
	void* data;
	vmaMapMemory(mvkobjs.rdallocator, ubodata[1].rdubobufferalloc, &data);
	std::memcpy(data, &texidx, ubodata[1].rduniformbuffersize);
	vmaUnmapMemory(mvkobjs.rdallocator, ubodata[1].rdubobufferalloc);
}


void ubo::cleanup(vkobjs& mvkobjs, std::vector<vkuniformbufferdata>& ubodata)
{
	for (int i{ 0 }; i < ubodata.size(); i++) {
		vkDestroyDescriptorPool(mvkobjs.rdvkbdevice.device, ubodata[i].rdubodescriptorpool, nullptr);
		vkDestroyDescriptorSetLayout(mvkobjs.rdvkbdevice.device, ubodata[i].rdubodescriptorlayout, nullptr);
		vmaDestroyBuffer(mvkobjs.rdallocator, ubodata[i].rdubobuffer, ubodata[i].rdubobufferalloc);
	}
}
