#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan.h>

#include "vkobjs.hpp"

namespace ssbo {
  static inline bool init(vkobjs &objs, ssbodata &ssboData, size_t buffersize) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = buffersize;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    VmaAllocationCreateInfo vmaAllocInfo{};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    if (vmaCreateBuffer(objs.alloc, &bufferInfo, &vmaAllocInfo, &ssboData.buffer, &ssboData.alloc, nullptr) !=
	VK_SUCCESS) {
      return false;
    }

    VkDescriptorSetLayoutBinding ssboBind{};
    ssboBind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    ssboBind.binding = 0;
    ssboBind.descriptorCount = 1;
    ssboBind.pImmutableSamplers = nullptr;
    ssboBind.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo ssboCreateInfo{};
    ssboCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ssboCreateInfo.bindingCount = 1;
    ssboCreateInfo.pBindings = &ssboBind;

    if (vkCreateDescriptorSetLayout(objs.vkdevice.device, &ssboCreateInfo, nullptr, &ssboData.dlayout) != VK_SUCCESS) {
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

    if (vkCreateDescriptorPool(objs.vkdevice.device, &descriptorPool, nullptr, &ssboData.dpool) != VK_SUCCESS) {
      return false;
    }

    VkDescriptorSetAllocateInfo descriptorAllocateInfo{};
    descriptorAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorAllocateInfo.descriptorPool = ssboData.dpool;
    descriptorAllocateInfo.descriptorSetCount = 1;
    descriptorAllocateInfo.pSetLayouts = &ssboData.dlayout;

    if (vkAllocateDescriptorSets(objs.vkdevice.device, &descriptorAllocateInfo, &ssboData.dset) != VK_SUCCESS) {
      return false;
    }

    VkDescriptorBufferInfo ssboInfo{};
    ssboInfo.buffer = ssboData.buffer;
    ssboInfo.offset = 0;
    ssboInfo.range = buffersize;

    VkWriteDescriptorSet writeDescriptorSet{};
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeDescriptorSet.dstSet = ssboData.dset;
    writeDescriptorSet.dstBinding = 0;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.pBufferInfo = &ssboInfo;

    vkUpdateDescriptorSets(objs.vkdevice.device, 1, &writeDescriptorSet, 0, nullptr);

    ssboData.size = buffersize;

    return true;
  }
  // template <typename T>
  // concept vectoradjacent = requires(T x){
  //   {x.size()} -> std::convertible_to<size_t>;
  //   {x.data()};
  // };
  template <typename T> 
  static inline void upload(const vkobjs &objs, const ssbodata &ssbodata, const std::vector<T> &mats) {
    if (mats.size() <= 0) {
      return;
    }
    void *data;
    vmaMapMemory(objs.alloc, ssbodata.alloc, &data);
    std::memcpy(data, mats.data(), ssbodata.size);
    vmaUnmapMemory(objs.alloc, ssbodata.alloc);
  }
static inline void cleanup(vkobjs &objs, ssbodata &ssbodata) {
    vkDestroyDescriptorPool(objs.vkdevice.device, ssbodata.dpool, nullptr);
    vkDestroyDescriptorSetLayout(objs.vkdevice.device, ssbodata.dlayout, nullptr);
    vmaDestroyBuffer(objs.alloc, ssbodata.buffer, ssbodata.alloc);
  }
};
