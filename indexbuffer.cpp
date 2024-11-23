#include <cstring>
#include "indexbuffer.hpp"
#include "commandbuffer.hpp"

bool indexbuffer::init(vkobjs& objs, vkindexbufferdata& indexbufferdata, size_t buffersize)
{
    VkBufferCreateInfo binfo{};
    binfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    binfo.size = buffersize;
    binfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    binfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo bainfo{};
    bainfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    if (vmaCreateBuffer(objs.rdallocator, &binfo, &bainfo, &indexbufferdata.rdindexbuffer, &indexbufferdata.rdindexbufferalloc, nullptr) != VK_SUCCESS)return false;

    VkBufferCreateInfo stagingbinfo{};
    stagingbinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingbinfo.size = buffersize;
    stagingbinfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo stagingallocinfo{};
    stagingallocinfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    if (vmaCreateBuffer(objs.rdallocator, &stagingbinfo, &stagingallocinfo, &indexbufferdata.rdstagingbuffer, &indexbufferdata.rdstagingbufferalloc, nullptr) != VK_SUCCESS)return false;



    indexbufferdata.rdindexbuffersize = buffersize;

    return true;
}


bool indexbuffer::upload(vkobjs& objs, vkindexbufferdata& indexbufferdata, const tinygltf::Buffer& buffer, const tinygltf::BufferView& bufferview, const tinygltf::Accessor& acc) {

    if (indexbufferdata.rdindexbuffersize < acc.count*2) {
        cleanup(objs, indexbufferdata);
        if (!init(objs, indexbufferdata, acc.count * 2))return false;
        indexbufferdata.rdindexbuffersize = acc.count * 2;
    }

    void* d;
    vmaMapMemory(objs.rdallocator, indexbufferdata.rdstagingbufferalloc, &d);
    std::memcpy(d, &buffer.data[bufferview.byteOffset+acc.byteOffset], acc.count * 2);
    vmaUnmapMemory(objs.rdallocator, indexbufferdata.rdstagingbufferalloc);

    VkBufferMemoryBarrier vbbarrier{};
    vbbarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    vbbarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    vbbarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    vbbarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vbbarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vbbarrier.buffer = indexbufferdata.rdstagingbuffer;
    vbbarrier.offset = 0;
    vbbarrier.size = indexbufferdata.rdindexbuffersize;


    VkBufferCopy stagingbuffercopy{};
    stagingbuffercopy.srcOffset = 0;
    stagingbuffercopy.dstOffset = 0;
    stagingbuffercopy.size = indexbufferdata.rdindexbuffersize;


    vkCmdCopyBuffer(objs.rdcommandbuffer[0], indexbufferdata.rdstagingbuffer, indexbufferdata.rdindexbuffer, 1, &stagingbuffercopy);
    vkCmdPipelineBarrier(objs.rdcommandbuffer[0], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &vbbarrier, 0, nullptr);





    return true;
}

bool indexbuffer::upload(vkobjs& objs, vkindexbufferdata& indexbufferdata, std::vector<unsigned short> indicez){

    void* d;
    vmaMapMemory(objs.rdallocator, indexbufferdata.rdstagingbufferalloc, &d);
    std::memcpy(d, indicez.data(), indicez.size() * sizeof(unsigned short));
    vmaUnmapMemory(objs.rdallocator, indexbufferdata.rdstagingbufferalloc);

    VkBufferMemoryBarrier vbbarrier{};
    vbbarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    vbbarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    vbbarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    vbbarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vbbarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vbbarrier.buffer = indexbufferdata.rdstagingbuffer;
    vbbarrier.offset = 0;
    vbbarrier.size = indexbufferdata.rdindexbuffersize;


    VkBufferCopy stagingbuffercopy{};
    stagingbuffercopy.srcOffset = 0;
    stagingbuffercopy.dstOffset = 0;
    stagingbuffercopy.size = indexbufferdata.rdindexbuffersize;


    vkCmdCopyBuffer(objs.rdcommandbuffer[0], indexbufferdata.rdstagingbuffer, indexbufferdata.rdindexbuffer, 1, &stagingbuffercopy);
    vkCmdPipelineBarrier(objs.rdcommandbuffer[0], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &vbbarrier, 0, nullptr);


    return true;
}





void indexbuffer::cleanup(vkobjs& objs, vkindexbufferdata& indexbufferdata) {
    vmaDestroyBuffer(objs.rdallocator, indexbufferdata.rdstagingbuffer, indexbufferdata.rdstagingbufferalloc);
    vmaDestroyBuffer(objs.rdallocator, indexbufferdata.rdindexbuffer, indexbufferdata.rdindexbufferalloc);
}