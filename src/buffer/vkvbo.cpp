#include "vkvbo.hpp"

bool vkvbo::init(rvkbucket &mvkobjs, GpuBuffer &vbdata, VkDeviceSize bsize, VkBufferUsageFlags usage) {
	VkBufferCreateInfo binfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	binfo.size = bsize;
	binfo.usage = usage;
	binfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo bainfo{};
	bainfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	if (vmaCreateBuffer(mvkobjs.alloc, &binfo, &bainfo, &vbdata.buffer, &vbdata.alloc, nullptr) != VK_SUCCESS) {
		std::cerr << "gpu out of memory probably, memory attempted to allocate: " << bsize << "\n";
		return false;
	}

	vbdata.size = bsize;

	return true;
}

VkBufferMemoryBarrier2 vkvbo::record_upload(rvkbucket &mvkobjs,
        VkCommandBuffer &cbuffer,
        GpuBuffer &targetBuffer,
        const fastgltf::Buffer &buffer,
        const fastgltf::BufferView &bufferview,
        const fastgltf::Accessor &acc,
        void* mappedStagingStart,
        VkBuffer stagingBufferHandle,
        VkDeviceSize stagingOffset)
{
	size_t elementSize = fastgltf::getElementByteSize(acc.type, acc.componentType);
	size_t copySize = acc.count * elementSize;

	if (targetBuffer.size < copySize) {
		return {};
	}

	// todo: add stupid byteview
	std::visit(fastgltf::visitor{[](auto &arg) {},
	[&](const fastgltf::sources::Array &vector) {
		const std::byte* src = vector.bytes.data() + bufferview.byteOffset + acc.byteOffset;

		std::byte* dst = static_cast<std::byte*>(mappedStagingStart) + stagingOffset;

		size_t stride = bufferview.byteStride.value_or(elementSize);

		if (stride == elementSize) {
			std::memcpy(dst, src, copySize);
		} else {
			for (size_t i = 0; i < acc.count; ++i) {
				std::memcpy(dst + (i * elementSize), src + (i * stride), elementSize);
			}
		}
	}}, buffer.data);


	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = stagingOffset;
	copyRegion.dstOffset = 0;
	copyRegion.size = copySize;

	vkCmdCopyBuffer(cbuffer, stagingBufferHandle, targetBuffer.buffer, 1, &copyRegion);

	VkBufferMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
	barrier.srcStageMask  = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	barrier.dstStageMask  = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
	barrier.dstAccessMask = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_2_INDEX_READ_BIT;
	barrier.buffer        = targetBuffer.buffer;
	barrier.size          = VK_WHOLE_SIZE;
	barrier.offset        = 0;

	return barrier;
}