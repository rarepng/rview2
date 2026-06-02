#include "ubo.hpp"
#include <VkBootstrap.h>

bool ubo::init_global(rvkbucket &mvkobjs) {
    rvkbucket::globalCameraUBO.size = (2 * sizeof(glm::mat4)) + sizeof(glm::vec4);
    
    VkBufferCreateInfo binfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    binfo.size = rvkbucket::globalCameraUBO.size;
    binfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    VmaAllocationCreateInfo vmaallocinfo{};
    vmaallocinfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    if (vmaCreateBuffer(mvkobjs.alloc, &binfo, &vmaallocinfo, 
                        &rvkbucket::globalCameraUBO.buffer, 
                        &rvkbucket::globalCameraUBO.alloc, nullptr) != VK_SUCCESS)
        return false;

    VkDescriptorBufferInfo uinfo{rvkbucket::globalCameraUBO.buffer, 0, rvkbucket::globalCameraUBO.size};

    VkWriteDescriptorSet writedset{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    writedset.dstSet = rvkbucket::globalBindlessSet;
    writedset.dstBinding = 1; 
    writedset.dstArrayElement = 0;
    writedset.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writedset.descriptorCount = 1;
    writedset.pBufferInfo = &uinfo;

    vkUpdateDescriptorSets(mvkobjs.vkdevice.device, 1, &writedset, 0, nullptr);
    return true;
}

void ubo::upload(rvkbucket &mvkobjs, std::vector<ssbodata> &ubodata, std::vector<glm::mat4> mats,const glm::vec3& campos) {
	//todo: fix this mess
	glm::vec4 campos4(campos, 0.0f);
	void* data;
	vmaMapMemory(mvkobjs.alloc, ubodata[0].alloc, &data);
	std::byte* dst = static_cast<std::byte*>(data);
	std::memcpy(dst, mats.data(), mats.size() * sizeof(glm::mat4));
	std::memcpy(dst+(mats.size() * sizeof(glm::mat4)), &campos4, sizeof(campos4));
	vmaUnmapMemory(mvkobjs.alloc, ubodata[0].alloc);
}

void ubo::cleanup(rvkbucket &mvkobjs, std::vector<ssbodata> &ubodata) {
	for (size_t i{0}; i < ubodata.size(); i++) {
		vmaDestroyBuffer(mvkobjs.alloc, ubodata[i].buffer, ubodata[i].alloc);
	}
}
