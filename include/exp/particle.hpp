#include "vkobjs.hpp"
namespace particle{
    static inline create(vkobjs objs){


        size_t ssbobuffersize{256*sizeof(float)*6};
        VkBuffer ssbobuffer = VK_NULL_HANDLE;
        VmaAllocation alloc = nullptr;

        VkDescriptorPool dpool = VK_NULL_HANDLE;
        VkDescriptorSetLayout dlayout = VK_NULL_HANDLE;
        VkDescriptorSet dset = VK_NULL_HANDLE;

        std::vector<VkDescriptorPoolSize> pool0{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1}};
        if(!rpool::create(pool0,objs.vkdevice.device,&dpool))return false;

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = buffersize;
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo vmaAllocInfo{};
        vmaAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        if (vmaCreateBuffer(objs.alloc, &bufferInfo, &vmaAllocInfo, &ssboData.buffer, &ssboData.alloc, nullptr) !=
        VK_SUCCESS) {
        return false;
        }

    }
}