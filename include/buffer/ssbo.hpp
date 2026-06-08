#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan.h>

#include "core/rvk.hpp"

namespace ssbo {
inline bool createmateriallayout(rvkbucket &core, VkDescriptorSetLayout& dlayout) {
	if (dlayout != VK_NULL_HANDLE) return true;

	VkDescriptorSetLayoutBinding matBind{};
	matBind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	matBind.binding = 0;
	matBind.descriptorCount = 1;
	matBind.pImmutableSamplers = nullptr;
	matBind.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	info.bindingCount = 1;
	info.pBindings = &matBind;

	return vkCreateDescriptorSetLayout(core.vkdevice.device, &info, nullptr, &dlayout) == VK_SUCCESS;
}
template <typename T>
inline void upload(const rvkbucket &objs, const ssbodata &ssbodata, const std::vector<T>& mats) {
	if (mats.size() <= 0) {
		return;
	}

	void* data;
	vmaMapMemory(objs.alloc, ssbodata.alloc, &data);
	std::memcpy(data, mats.data(), mats.size() * sizeof(T));
	vmaUnmapMemory(objs.alloc, ssbodata.alloc);
}
// "fast path"
template <typename T>
inline void upload(const rvkbucket &objs, const ssbodata &ssbodata, const T* data_ptr, size_t count) {
	if (count == 0 || data_ptr == nullptr) {
		return;
	}

	void* data;
	vmaMapMemory(objs.alloc, ssbodata.alloc, &data);
	std::memcpy(data, data_ptr, count * sizeof(T));
	vmaUnmapMemory(objs.alloc, ssbodata.alloc);
}
inline void cleanup(rvkbucket &objs, ssbodata &ssbodata) {
	vmaDestroyBuffer(objs.alloc, ssbodata.buffer, ssbodata.alloc);
}
inline bool init_bindless(rvkbucket &objs, ssbodata &ssboData, size_t buffersize, uint32_t modelID) {
	VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	bufferInfo.size = buffersize;
	bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	VmaAllocationCreateInfo vmaAllocInfo{};
	vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
	vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

	if (vmaCreateBuffer(objs.alloc, &bufferInfo, &vmaAllocInfo, &ssboData.buffer, &ssboData.alloc, nullptr) != VK_SUCCESS) {
		return false;
	}

	ssboData.size = buffersize;
	VkDescriptorBufferInfo ssboInfo{ssboData.buffer, 0, buffersize};

	VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	write.dstSet = rview::core::globalBindlessSet;
	write.dstBinding = 2;
	write.dstArrayElement = modelID;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	write.descriptorCount = 1;
	write.pBufferInfo = &ssboInfo;

	{
        std::lock_guard<std::shared_mutex> lock(*rview::core::mtx2);
        vkUpdateDescriptorSets(objs.vkdevice.device, 1, &write, 0, nullptr);
    }

	return true;
}
};
