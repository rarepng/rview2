#include "vkvbo.hpp"

bool vkvbo::init(vkobjs &mvkobjs, vbodata &vbdata, size_t bsize) {
	VkBufferCreateInfo binfo{};
	binfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	binfo.size = bsize;
	binfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	binfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo bainfo{};
	bainfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	if (vmaCreateBuffer(mvkobjs.alloc, &binfo, &bainfo, &vbdata.buffer, &vbdata.alloc,
	                    nullptr) != VK_SUCCESS)
		return false;

	VkBufferCreateInfo stagingbinfo{};
	stagingbinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	stagingbinfo.size = bsize;
	stagingbinfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	VmaAllocationCreateInfo stagingainfo{};
	stagingainfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

	if (vmaCreateBuffer(mvkobjs.alloc, &stagingbinfo, &stagingainfo, &vbdata.sbuffer, &vbdata.salloc,
	                    nullptr) != VK_SUCCESS)
		return false;

	vbdata.size = bsize;
	return true;
}

bool vkvbo::upload(vkobjs &mvkobjs, VkCommandBuffer &cbuffer, vbodata &vbdata,
                   std::vector<glm::vec3> vertexData) {

	/* copy data to staging buffer*/
	void *data;
	vmaMapMemory(mvkobjs.alloc, vbdata.salloc, &data);
	std::memcpy(data, vertexData.data(), vbdata.size);
	vmaUnmapMemory(mvkobjs.alloc, vbdata.salloc);

	VkBufferMemoryBarrier vertexBufferBarrier{};
	vertexBufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	vertexBufferBarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
	vertexBufferBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
	vertexBufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	vertexBufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	vertexBufferBarrier.buffer = vbdata.sbuffer;
	vertexBufferBarrier.offset = 0;
	vertexBufferBarrier.size = vbdata.size;

	VkBufferCopy stagingBufferCopy{};
	stagingBufferCopy.srcOffset = 0;
	stagingBufferCopy.dstOffset = 0;
	stagingBufferCopy.size = vbdata.size;

	vkCmdCopyBuffer(cbuffer, vbdata.sbuffer, vbdata.buffer, 1, &stagingBufferCopy);
	vkCmdPipelineBarrier(cbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1,
	                     &vertexBufferBarrier, 0, nullptr);

	return true;
}

bool vkvbo::upload(vkobjs &mvkobjs, VkCommandBuffer &cbuffer, vbodata &vbdata,
                   std::vector<glm::vec2> vertexData) {

	void *data;
	vmaMapMemory(mvkobjs.alloc, vbdata.salloc, &data);
	std::memcpy(data, vertexData.data(), vbdata.size);
	vmaUnmapMemory(mvkobjs.alloc, vbdata.salloc);

	VkBufferMemoryBarrier vertexBufferBarrier{};
	vertexBufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	vertexBufferBarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
	vertexBufferBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
	vertexBufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	vertexBufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	vertexBufferBarrier.buffer = vbdata.sbuffer;
	vertexBufferBarrier.offset = 0;
	vertexBufferBarrier.size = vbdata.size;

	VkBufferCopy stagingBufferCopy{};
	stagingBufferCopy.srcOffset = 0;
	stagingBufferCopy.dstOffset = 0;
	stagingBufferCopy.size = vbdata.size;

	vkCmdCopyBuffer(cbuffer, vbdata.sbuffer, vbdata.buffer, 1, &stagingBufferCopy);
	vkCmdPipelineBarrier(cbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1,
	                     &vertexBufferBarrier, 0, nullptr);

	return true;
}

bool vkvbo::upload(vkobjs &mvkobjs, VkCommandBuffer &cbuffer, vbodata &vbdata,
                   const fastgltf::Buffer &buffer, const fastgltf::BufferView &bufferview,
                   const fastgltf::Accessor &acc) {

	std::visit(fastgltf::visitor{[](auto &arg) {},
	[&](const fastgltf::sources::Array &vector) {
		void *d;
		vmaMapMemory(mvkobjs.alloc, vbdata.salloc, &d);
		std::memcpy(d, vector.bytes.data() + bufferview.byteOffset + acc.byteOffset,
		            bufferview.byteLength); // acc.count*type
		vmaUnmapMemory(mvkobjs.alloc, vbdata.salloc);
	}},
	buffer.data);

	VkBufferMemoryBarrier vbbarrier{};
	vbbarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	vbbarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
	vbbarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
	vbbarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	vbbarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	vbbarrier.buffer = vbdata.sbuffer;
	vbbarrier.offset = 0;
	vbbarrier.size = vbdata.size;

	VkBufferCopy stagingbuffercopy{};
	stagingbuffercopy.srcOffset = 0;
	stagingbuffercopy.dstOffset = 0;
	stagingbuffercopy.size = vbdata.size;

	vkCmdCopyBuffer(cbuffer, vbdata.sbuffer, vbdata.buffer, 1, &stagingbuffercopy);
	vkCmdPipelineBarrier(cbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1,
	                     &vbbarrier, 0, nullptr);

	return true;
}

bool vkvbo::upload(vkobjs &mvkobjs, VkCommandBuffer &cbuffer, vbodata &vbdata,
                   const std::vector<unsigned int> &jointz, const unsigned int count, const unsigned int ofx) {

	void *d;

	vmaMapMemory(mvkobjs.alloc, vbdata.salloc, &d);
	std::memcpy(d, jointz.data() + ofx, count * sizeof(unsigned int) * 4);
	vmaUnmapMemory(mvkobjs.alloc, vbdata.salloc);

	VkBufferMemoryBarrier vbbarrier{};
	vbbarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	vbbarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
	vbbarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
	vbbarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	vbbarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	vbbarrier.buffer = vbdata.sbuffer;
	vbbarrier.offset = 0;
	vbbarrier.size = vbdata.size;

	VkBufferCopy stagingbuffercopy{};
	stagingbuffercopy.srcOffset = 0;
	stagingbuffercopy.dstOffset = 0;
	stagingbuffercopy.size = vbdata.size;

	vkCmdCopyBuffer(cbuffer, vbdata.sbuffer, vbdata.buffer, 1, &stagingbuffercopy);
	vkCmdPipelineBarrier(cbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1,
	                     &vbbarrier, 0, nullptr);

	return true;
}

void vkvbo::cleanup(vkobjs &mvkobjs, vbodata &vbdata) {
	vmaDestroyBuffer(mvkobjs.alloc, vbdata.sbuffer, vbdata.salloc);
	vmaDestroyBuffer(mvkobjs.alloc, vbdata.buffer, vbdata.alloc);
}
