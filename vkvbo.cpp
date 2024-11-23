#include "vkvbo.hpp"
#include "commandbuffer.hpp"

bool vkvbo::init(vkobjs& mvkobjs, vkvertexbufferdata& vbdata, size_t bsize)
{
    VkBufferCreateInfo binfo{};
    binfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    binfo.size = bsize;
    binfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    binfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo bainfo{};
    bainfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    if (vmaCreateBuffer(mvkobjs.rdallocator, &binfo, &bainfo, &vbdata.rdvertexbuffer, &vbdata.rdvertexbufferalloc, nullptr) != VK_SUCCESS)return false;


    VkBufferCreateInfo stagingbinfo{};
    stagingbinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingbinfo.size = bsize;
    stagingbinfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo stagingainfo{};
    stagingainfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    if (vmaCreateBuffer(mvkobjs.rdallocator, &stagingbinfo, &stagingainfo, &vbdata.rdstagingbuffer, &vbdata.rdstagingbufferalloc, nullptr) != VK_SUCCESS)return false;


    vbdata.rdvertexbuffersize = bsize;
    return true;
}


bool vkvbo::upload(vkobjs& mvkobjs, vkvertexbufferdata& vbdata, vkmesh vmesh){
    size_t vsize = vmesh.verts.size() * sizeof(vkvert);

    if (vbdata.rdvertexbuffersize < vsize) {
        cleanup(mvkobjs, vbdata);
        if (!init(mvkobjs, vbdata,vsize)) return false;
        vbdata.rdvertexbuffersize = vsize;
    }

    void* d;

    vmaMapMemory(mvkobjs.rdallocator, vbdata.rdstagingbufferalloc, &d);
    std::memcpy(d, vmesh.verts.data(), vsize);
    vmaUnmapMemory(mvkobjs.rdallocator, vbdata.rdstagingbufferalloc);

    VkBufferMemoryBarrier vbbarrier{};
    vbbarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    vbbarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    vbbarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    vbbarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vbbarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vbbarrier.buffer = vbdata.rdstagingbuffer;
    vbbarrier.offset = 0;
    vbbarrier.size = vbdata.rdvertexbuffersize;

    VkBufferCopy stagingbuffercopy{};
    stagingbuffercopy.srcOffset = 0;
    stagingbuffercopy.dstOffset = 0;
    stagingbuffercopy.size = vbdata.rdvertexbuffersize;



    vkCmdCopyBuffer(mvkobjs.rdcommandbuffer[0], vbdata.rdstagingbuffer,
        vbdata.rdvertexbuffer, 1, &stagingbuffercopy);
    vkCmdPipelineBarrier(mvkobjs.rdcommandbuffer[0], VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &vbbarrier, 0, nullptr);

    return true;
}

bool vkvbo::upload(vkobjs& mvkobjs, vkvertexbufferdata& vbdata,std::vector<glm::vec3> vertexData) {

    /* copy data to staging buffer*/
    void* data;
    vmaMapMemory(mvkobjs.rdallocator, vbdata.rdstagingbufferalloc, &data);
    std::memcpy(data, vertexData.data(), vbdata.rdvertexbuffersize);
    vmaUnmapMemory(mvkobjs.rdallocator, vbdata.rdstagingbufferalloc);

    VkBufferMemoryBarrier vertexBufferBarrier{};
    vertexBufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    vertexBufferBarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    vertexBufferBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    vertexBufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vertexBufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vertexBufferBarrier.buffer = vbdata.rdstagingbuffer;
    vertexBufferBarrier.offset = 0;
    vertexBufferBarrier.size = vbdata.rdvertexbuffersize;

    VkBufferCopy stagingBufferCopy{};
    stagingBufferCopy.srcOffset = 0;
    stagingBufferCopy.dstOffset = 0;
    stagingBufferCopy.size = vbdata.rdvertexbuffersize;



    vkCmdCopyBuffer(mvkobjs.rdcommandbuffer[0], vbdata.rdstagingbuffer, vbdata.rdvertexbuffer, 1, &stagingBufferCopy);
    vkCmdPipelineBarrier(mvkobjs.rdcommandbuffer[0], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &vertexBufferBarrier, 0, nullptr);
    
    return true;
}








bool vkvbo::upload(vkobjs& mvkobjs, vkvertexbufferdata& vbdata, const tinygltf::Buffer& buffer, const tinygltf::BufferView& bufferview, const tinygltf::Accessor& acc) {

    //if (vbdata.rdvertexbuffersize < acc.count*bufferview.byteStride) {
    //    cleanup(mvkobjs, vbdata);
    //    if (!init(mvkobjs, vbdata, acc.count * bufferview.byteStride)) return false;
    //    vbdata.rdvertexbuffersize = acc.count * bufferview.byteStride;
    //}

    //void* d;

    ////if (bufferview.byteStride == 4) {
    ////    std::vector<glm::tvec4<unsigned char>> xxxx{};
    ////    xxxx.reserve(9070);
    ////    xxxx.resize(9070);
    ////    std::cout << sizeof(glm::tvec4<unsigned char>);
    ////    std::memcpy(xxxx.data(), &buffer.data[bufferview.byteOffset + acc.byteOffset], acc.count * bufferview.byteStride);
    ////}
    //vmaMapMemory(mvkobjs.rdallocator, vbdata.rdstagingbufferalloc, &d);
    //std::memcpy(d, &buffer.data[bufferview.byteOffset+acc.byteOffset], acc.count * bufferview.byteStride);
    //vmaUnmapMemory(mvkobjs.rdallocator, vbdata.rdstagingbufferalloc);


    if (vbdata.rdvertexbuffersize < bufferview.byteLength) {
        cleanup(mvkobjs, vbdata);
        if (!init(mvkobjs, vbdata, bufferview.byteLength)) return false;
        vbdata.rdvertexbuffersize = bufferview.byteLength;
    }

    void* d;

    //if (bufferview.byteStride == 4) {
    //    std::vector<glm::tvec4<unsigned char>> xxxx{};
    //    xxxx.reserve(9070);
    //    xxxx.resize(9070);
    //    std::cout << sizeof(glm::tvec4<unsigned char>);
    //    std::memcpy(xxxx.data(), &buffer.data[bufferview.byteOffset + acc.byteOffset], acc.count * bufferview.byteStride);
    //}
    vmaMapMemory(mvkobjs.rdallocator, vbdata.rdstagingbufferalloc, &d);
    std::memcpy(d, &buffer.data[bufferview.byteOffset + acc.byteOffset], bufferview.byteLength);
    vmaUnmapMemory(mvkobjs.rdallocator, vbdata.rdstagingbufferalloc);



    VkBufferMemoryBarrier vbbarrier{};
    vbbarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    vbbarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    vbbarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    vbbarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vbbarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vbbarrier.buffer = vbdata.rdstagingbuffer;
    vbbarrier.offset = 0;
    vbbarrier.size = vbdata.rdvertexbuffersize;

    VkBufferCopy stagingbuffercopy{};
    stagingbuffercopy.srcOffset = 0;
    stagingbuffercopy.dstOffset = 0;
    stagingbuffercopy.size = vbdata.rdvertexbuffersize;

    vkCmdCopyBuffer(mvkobjs.rdcommandbuffer[0], vbdata.rdstagingbuffer, vbdata.rdvertexbuffer, 1, &stagingbuffercopy);
    vkCmdPipelineBarrier(mvkobjs.rdcommandbuffer[0], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &vbbarrier, 0, nullptr);

    return true;
}








void vkvbo::cleanup(vkobjs& mvkobjs,vkvertexbufferdata& vbdata) {
    vmaDestroyBuffer(mvkobjs.rdallocator, vbdata.rdstagingbuffer, vbdata.rdstagingbufferalloc);
    vmaDestroyBuffer(mvkobjs.rdallocator, vbdata.rdvertexbuffer, vbdata.rdvertexbufferalloc);
}