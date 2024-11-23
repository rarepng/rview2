#include "ssbo.hpp"

#include <vk/VkBootstrap.h>

bool ssbo::init(vkobjs& objs, vkshaderstoragebufferdata& SSBOData,
    size_t bufferSize) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    VmaAllocationCreateInfo vmaAllocInfo{};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    if (vmaCreateBuffer(objs.rdallocator, &bufferInfo, &vmaAllocInfo,
        &SSBOData.rdssbobuffer, &SSBOData.rdssbobufferalloc, nullptr) != VK_SUCCESS) {
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

    if (vkCreateDescriptorSetLayout(objs.rdvkbdevice.device, &ssboCreateInfo, nullptr,
        &SSBOData.rdssbodescriptorlayout) != VK_SUCCESS) {
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

    if (vkCreateDescriptorPool(objs.rdvkbdevice.device, &descriptorPool, nullptr,
        &SSBOData.rdssbodescriptorpool) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorSetAllocateInfo descriptorAllocateInfo{};
    descriptorAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorAllocateInfo.descriptorPool = SSBOData.rdssbodescriptorpool;
    descriptorAllocateInfo.descriptorSetCount = 1;
    descriptorAllocateInfo.pSetLayouts = &SSBOData.rdssbodescriptorlayout;

    if (vkAllocateDescriptorSets(objs.rdvkbdevice.device, &descriptorAllocateInfo,
        &SSBOData.rdssbodescriptorset) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorBufferInfo ssboInfo{};
    ssboInfo.buffer = SSBOData.rdssbobuffer;
    ssboInfo.offset = 0;
    ssboInfo.range = bufferSize;

    VkWriteDescriptorSet writeDescriptorSet{};
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeDescriptorSet.dstSet = SSBOData.rdssbodescriptorset;
    writeDescriptorSet.dstBinding = 0;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.pBufferInfo = &ssboInfo;

    vkUpdateDescriptorSets(objs.rdvkbdevice.device, 1, &writeDescriptorSet, 0, nullptr);

    SSBOData.rdssbobuffersize = bufferSize;
    return true;
}

void ssbo::upload(vkobjs& objs,vkshaderstoragebufferdata& ssbodata, std::vector<glm::mat4> mats) {
    if (mats.size() == 0) {
        return;
    }

    void* data;
    vmaMapMemory(objs.rdallocator, ssbodata.rdssbobufferalloc, &data);
    std::memcpy(data, mats.data(), ssbodata.rdssbobuffersize);
    vmaUnmapMemory(objs.rdallocator, ssbodata.rdssbobufferalloc);
}

void ssbo::upload(vkobjs& objs,vkshaderstoragebufferdata& ssbodata, std::vector<glm::mat2x4> mats) {
    if (mats.size() == 0) {
        return;
    }

    void* data;
    vmaMapMemory(objs.rdallocator, ssbodata.rdssbobufferalloc, &data);
    std::memcpy(data, mats.data(), ssbodata.rdssbobuffersize);
    vmaUnmapMemory(objs.rdallocator, ssbodata.rdssbobufferalloc);
}

void ssbo::cleanup(vkobjs& objs, vkshaderstoragebufferdata& ssbodata) {
    vkDestroyDescriptorPool(objs.rdvkbdevice.device, ssbodata.rdssbodescriptorpool,
        nullptr);
    vkDestroyDescriptorSetLayout(objs.rdvkbdevice.device, ssbodata.rdssbodescriptorlayout,
        nullptr);
    vmaDestroyBuffer(objs.rdallocator, ssbodata.rdssbobuffer, ssbodata.rdssbobufferalloc);
}
