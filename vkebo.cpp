#include <cstring>
#include "vkebo.hpp"
#include "commandbuffer.hpp"

bool vkebo::init(vkobjs& objs, vkebodata& indexbufferdata, size_t buffersize)
{
    VkBufferCreateInfo binfo{};
    binfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    binfo.size = buffersize;
    binfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    binfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo bainfo{};
    bainfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    if (vmaCreateBuffer(objs.rdallocator, &binfo, &bainfo, &indexbufferdata.bhandle, &indexbufferdata.balloc, nullptr) != VK_SUCCESS)return false;

    VkBufferCreateInfo stagingbinfo{};
    stagingbinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingbinfo.size = buffersize;
    stagingbinfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo stagingallocinfo{};
    stagingallocinfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    if (vmaCreateBuffer(objs.rdallocator, &stagingbinfo, &stagingallocinfo, &indexbufferdata.stagingbhandle, &indexbufferdata.stagingballoc, nullptr) != VK_SUCCESS)return false;



    indexbufferdata.bsize = buffersize;

    return true;
}


//bool vkebo::upload(vkobjs& objs, vkindexbufferdata& indexbufferdata, const tinygltf::Buffer& buffer, const tinygltf::BufferView& bufferview, const tinygltf::Accessor& acc) {
//
//
//
//    void* d;
//    vmaMapMemory(objs.rdallocator, indexbufferdata.stagingballoc, &d);
//    std::memcpy(d, &buffer.data[bufferview.byteOffset+acc.byteOffset], acc.count * 2);
//    vmaUnmapMemory(objs.rdallocator, indexbufferdata.stagingballoc);
//
//    VkBufferMemoryBarrier vbbarrier{};
//    vbbarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
//    vbbarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
//    vbbarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
//    vbbarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//    vbbarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//    vbbarrier.buffer = indexbufferdata.stagingbhandle;
//    vbbarrier.offset = 0;
//    vbbarrier.size = indexbufferdata.bsize;
//
//
//    VkBufferCopy stagingbuffercopy{};
//    stagingbuffercopy.srcOffset = 0;
//    stagingbuffercopy.dstOffset = 0;
//    stagingbuffercopy.size = indexbufferdata.bsize;
//
//
//    vkCmdCopyBuffer(objs.rdcommandbuffer[0], indexbufferdata.stagingbhandle, indexbufferdata.bhandle, 1, &stagingbuffercopy);
//    vkCmdPipelineBarrier(objs.rdcommandbuffer[0], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &vbbarrier, 0, nullptr);
//
//
//
//
//
//    return true;
//}

bool vkebo::upload(vkobjs& objs, VkCommandBuffer& cbuffer, vkebodata& indexbufferdata, std::vector<unsigned short> indicez){

    void* d;
    vmaMapMemory(objs.rdallocator, indexbufferdata.stagingballoc, &d);
    std::memcpy(d, indicez.data(), indicez.size() * sizeof(unsigned short));
    vmaUnmapMemory(objs.rdallocator, indexbufferdata.stagingballoc);

    VkBufferMemoryBarrier vbbarrier{};
    vbbarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    vbbarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    vbbarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    vbbarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vbbarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vbbarrier.buffer = indexbufferdata.stagingbhandle;
    vbbarrier.offset = 0;
    vbbarrier.size = indexbufferdata.bsize;


    VkBufferCopy stagingbuffercopy{};
    stagingbuffercopy.srcOffset = 0;
    stagingbuffercopy.dstOffset = 0;
    stagingbuffercopy.size = indexbufferdata.bsize;


    vkCmdCopyBuffer(cbuffer, indexbufferdata.stagingbhandle, indexbufferdata.bhandle, 1, &stagingbuffercopy);
    vkCmdPipelineBarrier(cbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &vbbarrier, 0, nullptr);


    return true;
}


bool vkebo::upload(vkobjs& objs, VkCommandBuffer& cbuffer, vkebodata& indexbufferdata, const fastgltf::Buffer& buffer, const fastgltf::BufferView& bufferview, const size_t& count) {


    std::visit(fastgltf::visitor {
        [](auto& arg) {},
        [&](const fastgltf::sources::Array& vector) {
            void* d;
            vmaMapMemory(objs.rdallocator, indexbufferdata.stagingballoc, &d);
            std::memcpy(d, vector.bytes.data() + bufferview.byteOffset, count * 2);//bufferview.byteLength
            vmaUnmapMemory(objs.rdallocator, indexbufferdata.stagingballoc);
        } },buffer.data);


    VkBufferMemoryBarrier vbbarrier{};
    vbbarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    vbbarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    vbbarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    vbbarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vbbarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vbbarrier.buffer = indexbufferdata.stagingbhandle;
    vbbarrier.offset = 0;
    vbbarrier.size = indexbufferdata.bsize;


    VkBufferCopy stagingbuffercopy{};
    stagingbuffercopy.srcOffset = 0;
    stagingbuffercopy.dstOffset = 0;
    stagingbuffercopy.size = indexbufferdata.bsize;


    vkCmdCopyBuffer(cbuffer, indexbufferdata.stagingbhandle, indexbufferdata.bhandle, 1, &stagingbuffercopy);
    vkCmdPipelineBarrier(cbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &vbbarrier, 0, nullptr);





    return true;
}



void vkebo::cleanup(vkobjs& objs, vkebodata& indexbufferdata) {
    vmaDestroyBuffer(objs.rdallocator, indexbufferdata.stagingbhandle, indexbufferdata.stagingballoc);
    vmaDestroyBuffer(objs.rdallocator, indexbufferdata.bhandle, indexbufferdata.balloc);
}
