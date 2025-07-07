#include "vkebo.hpp"
#include <cstring>

bool vkebo::init(rvk &objs, ebodata &indexbufferdata, size_t buffersize) {
	VkBufferCreateInfo binfo{};
	binfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	binfo.size = buffersize;
	binfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	binfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo bainfo{};
	bainfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	if (vmaCreateBuffer(objs.alloc, &binfo, &bainfo, &indexbufferdata.buffer, &indexbufferdata.alloc, nullptr) !=
	        VK_SUCCESS)
		return false;

	VkBufferCreateInfo stagingbinfo{};
	stagingbinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	stagingbinfo.size = buffersize;
	stagingbinfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	VmaAllocationCreateInfo stagingallocinfo{};
	stagingallocinfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

	if (vmaCreateBuffer(objs.alloc, &stagingbinfo, &stagingallocinfo, &indexbufferdata.sbuffer,
	                    &indexbufferdata.salloc, nullptr) != VK_SUCCESS)
		return false;

	indexbufferdata.size = buffersize;

	return true;
}

bool vkebo::upload(rvk &objs, VkCommandBuffer &cbuffer, ebodata &indexbufferdata,
                   std::vector<unsigned short> indicez) {

	void *d;
	vmaMapMemory(objs.alloc, indexbufferdata.salloc, &d);
	std::memcpy(d, indicez.data(), indicez.size() * sizeof(unsigned short));
	vmaUnmapMemory(objs.alloc, indexbufferdata.salloc);

	VkBufferMemoryBarrier vbbarrier{};
	vbbarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	vbbarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
	vbbarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
	vbbarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	vbbarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	vbbarrier.buffer = indexbufferdata.sbuffer;
	vbbarrier.offset = 0;
	vbbarrier.size = indexbufferdata.size;

	VkBufferCopy stagingbuffercopy{};
	stagingbuffercopy.srcOffset = 0;
	stagingbuffercopy.dstOffset = 0;
	stagingbuffercopy.size = indexbufferdata.size;

	vkCmdCopyBuffer(cbuffer, indexbufferdata.sbuffer, indexbufferdata.buffer, 1, &stagingbuffercopy);
	vkCmdPipelineBarrier(cbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1,
	                     &vbbarrier, 0, nullptr);

	return true;
}

bool vkebo::upload(rvk &objs, VkCommandBuffer &cbuffer, ebodata &indexbufferdata, const fastgltf::Buffer &buffer,
                   const fastgltf::BufferView &bufferview, const size_t &count) {

	std::visit(fastgltf::visitor{[](auto &arg) {},
	[&](const fastgltf::sources::Array &vector) {
		void *d;
		vmaMapMemory(objs.alloc, indexbufferdata.salloc, &d);
		std::memcpy(d, vector.bytes.data() + bufferview.byteOffset,
		            count * 2); // bufferview.byteLength
		vmaUnmapMemory(objs.alloc, indexbufferdata.salloc);
	}},
	buffer.data);

	VkBufferMemoryBarrier vbbarrier{};
	vbbarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	vbbarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
	vbbarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
	vbbarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	vbbarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	vbbarrier.buffer = indexbufferdata.sbuffer;
	vbbarrier.offset = 0;
	vbbarrier.size = indexbufferdata.size;

	VkBufferCopy stagingbuffercopy{};
	stagingbuffercopy.srcOffset = 0;
	stagingbuffercopy.dstOffset = 0;
	stagingbuffercopy.size = indexbufferdata.size;

	vkCmdCopyBuffer(cbuffer, indexbufferdata.sbuffer, indexbufferdata.buffer, 1, &stagingbuffercopy);
	vkCmdPipelineBarrier(cbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1,
	                     &vbbarrier, 0, nullptr);

	return true;
}

void vkebo::cleanup(rvk &objs, ebodata &indexbufferdata) {
	vmaDestroyBuffer(objs.alloc, indexbufferdata.sbuffer, indexbufferdata.salloc);
	vmaDestroyBuffer(objs.alloc, indexbufferdata.buffer, indexbufferdata.alloc);
}
