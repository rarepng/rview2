#pragma once
#include <core/rvk.hpp>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
namespace ubo {
	inline bool init_global(rvkbucket& mvkobjs) {
		size_t cameraUBOSize = (2 * sizeof(glm::mat4)) + sizeof(glm::vec4);

		for (uint32_t i = 0; i < rview::core::MAX_FRAMES_IN_FLIGHT; ++i) {
			rview::core::globalCameraUBO[i].size = cameraUBOSize;

			VkBufferCreateInfo binfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
			binfo.size = cameraUBOSize;
			binfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

			VmaAllocationCreateInfo vmaallocinfo{};
			vmaallocinfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

			if (vmaCreateBuffer(
					mvkobjs.alloc, &binfo, &vmaallocinfo, &rview::core::globalCameraUBO[i].buffer, &rview::core::globalCameraUBO[i].alloc, nullptr) !=
				VK_SUCCESS) {
				return false;
			}

			VkDescriptorBufferInfo uinfo{rview::core::globalCameraUBO[i].buffer, 0, cameraUBOSize};

			VkWriteDescriptorSet writedset{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
			writedset.dstSet = rview::core::globalBindlessSet;
			writedset.dstBinding = 1;
			writedset.dstArrayElement = i;
			writedset.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writedset.descriptorCount = 1;
			writedset.pBufferInfo = &uinfo;

			vkUpdateDescriptorSets(mvkobjs.vkdevice.device, 1, &writedset, 0, nullptr);
		}

		return true;
	}

	inline void upload(rvkbucket& mvkobjs, std::vector<ssbodata>& ubodata, std::vector<glm::mat4> mats, const glm::vec3& campos) {
		//todo: fix this mess
		glm::vec4 campos4(campos, 0.0f);
		void* data;
		vmaMapMemory(mvkobjs.alloc, ubodata[0].alloc, &data);
		std::byte* dst = static_cast<std::byte*>(data);
		std::memcpy(dst, mats.data(), mats.size() * sizeof(glm::mat4));
		std::memcpy(dst + (mats.size() * sizeof(glm::mat4)), &campos4, sizeof(campos4));
		vmaUnmapMemory(mvkobjs.alloc, ubodata[0].alloc);
	}

	inline void cleanup(rvkbucket& mvkobjs, std::vector<ssbodata>& ubodata) {
		for (size_t i{0}; i < ubodata.size(); i++) {
			vmaDestroyBuffer(mvkobjs.alloc, ubodata[i].buffer, ubodata[i].alloc);
		}
	}

};
