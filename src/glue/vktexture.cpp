#define STB_IMAGE_IMPLEMENTATION
#include "vktexture.hpp"
#include "vk/commandbuffer.hpp"
#include "logger.hpp"
#include <VkBootstrap.h>
#include <cstring>
#include <stb_image.h>
#include <iostream>

bool vktexture::loadtexture(rvk &rdata, std::vector<texdata> &texdata, fastgltf::Asset &mmodel) {

	texdata.reserve(mmodel.images.size());
	texdata.resize(mmodel.images.size());
	for (size_t i{0}; i < mmodel.images.size(); i++) {

		int w, h, c;

		unsigned char *data = nullptr;

		std::visit(fastgltf::visitor{[](auto &arg) {},
		[&](fastgltf::sources::BufferView &view) {
			auto &bufferView = mmodel.bufferViews[view.bufferViewIndex];
			auto &buffer = mmodel.buffers[bufferView.bufferIndex];
			std::visit(fastgltf::visitor{[](auto &arg) {},
			[&](fastgltf::sources::Array &vector) {
				data = stbi_load_from_memory(
				           reinterpret_cast<const stbi_uc *>(
				               vector.bytes.data() + bufferView.byteOffset),
				           static_cast<int>(bufferView.byteLength), &w, &h,
				           &c, 4);
			}},
			buffer.data);
		}},
		mmodel.images[i].data);


		VkDeviceSize imgsize = w * h * 4;
		VkImageCreateInfo imginfo{};
		imginfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imginfo.imageType = VK_IMAGE_TYPE_2D;
		imginfo.extent.width = static_cast<uint32_t>(w);
		imginfo.extent.height = static_cast<uint32_t>(h);
		imginfo.extent.depth = 1;
		imginfo.mipLevels = 1;
		imginfo.arrayLayers = 1;
		imginfo.format = VK_FORMAT_R8G8B8A8_SRGB;
		imginfo.tiling = VK_IMAGE_TILING_LINEAR;
		imginfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imginfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imginfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imginfo.samples = VK_SAMPLE_COUNT_1_BIT;

		VmaAllocationCreateInfo iainfo{};
		iainfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		if (vmaCreateImage(rdata.alloc, &imginfo, &iainfo, &texdata[i].img, &texdata[i].alloc, nullptr) !=
		        VK_SUCCESS) {
			logger::log(0, "crashed in texture at vmaCreateImage");
			return false;
		}

		VkBufferCreateInfo stagingbufferinfo{};
		stagingbufferinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		stagingbufferinfo.size = imgsize;
		stagingbufferinfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		VkBuffer stagingbuffer;
		VmaAllocation stagingbufferalloc;

		VmaAllocationCreateInfo stagingallocinfo{};
		stagingallocinfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

		if (vmaCreateBuffer(rdata.alloc, &stagingbufferinfo, &stagingallocinfo, &stagingbuffer, &stagingbufferalloc,
		                    nullptr) != VK_SUCCESS) {
			logger::log(0, "crashed in texture at vmaCreateBuffer");
			return false;
		}

		void *tmp;
		vmaMapMemory(rdata.alloc, stagingbufferalloc, &tmp);
		std::memcpy(
		    tmp, data,
		    (imgsize)); ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		vmaUnmapMemory(rdata.alloc, stagingbufferalloc);

		stbi_image_free(data);

		VkImageSubresourceRange stagingbufferrange{};
		stagingbufferrange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		stagingbufferrange.baseMipLevel = 0;
		stagingbufferrange.levelCount = 1;
		stagingbufferrange.baseArrayLayer = 0;
		stagingbufferrange.layerCount = 1;

		VkImageMemoryBarrier stagingbuffertransferbarrier{};
		stagingbuffertransferbarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		stagingbuffertransferbarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		stagingbuffertransferbarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		stagingbuffertransferbarrier.image = texdata[i].img;
		stagingbuffertransferbarrier.subresourceRange = stagingbufferrange;
		stagingbuffertransferbarrier.srcAccessMask = 0;
		stagingbuffertransferbarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		VkExtent3D texextent{};
		texextent.width = static_cast<uint32_t>(w);
		texextent.height = static_cast<uint32_t>(h);
		texextent.depth = 1;

		VkBufferImageCopy stagingbuffercopy{};
		stagingbuffercopy.bufferOffset = 0;
		stagingbuffercopy.bufferRowLength = 0;
		stagingbuffercopy.bufferImageHeight = 0;
		stagingbuffercopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		stagingbuffercopy.imageSubresource.mipLevel = 0;
		stagingbuffercopy.imageSubresource.baseArrayLayer = 0;
		stagingbuffercopy.imageSubresource.layerCount = 1;
		stagingbuffercopy.imageExtent = texextent;

		VkImageMemoryBarrier stagingbuffershaderbarrier{};
		stagingbuffershaderbarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		stagingbuffershaderbarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		stagingbuffershaderbarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		stagingbuffershaderbarrier.image = texdata[i].img;
		stagingbuffershaderbarrier.subresourceRange = stagingbufferrange;
		stagingbuffershaderbarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		stagingbuffershaderbarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		std::array<VkCommandBuffer,1> stagingcommandbuffer;
		if (!commandbuffer::create(rdata, rdata.cpools_graphics.at(1), stagingcommandbuffer)) {
			logger::log(0, "crashed in texture at commandbuffer::init");
			return false;
		}

		if (vkResetCommandBuffer(stagingcommandbuffer.at(0), 0) != VK_SUCCESS) {
			logger::log(0, "crashed in texture at vkResetCommandBuffer");
			return false;
		}

		VkCommandBufferBeginInfo cmdbegininfo{};
		cmdbegininfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdbegininfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		if (vkBeginCommandBuffer(stagingcommandbuffer.at(0), &cmdbegininfo) != VK_SUCCESS) {
			logger::log(0, "crashed in texture at vkBeginCommandBuffer");
			return false;
		}

		vkCmdPipelineBarrier(stagingcommandbuffer.at(0), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
		                     nullptr, 0, nullptr, 1, &stagingbuffertransferbarrier);
		vkCmdCopyBufferToImage(stagingcommandbuffer.at(0), stagingbuffer, texdata[i].img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		                       1, &stagingbuffercopy);
		vkCmdPipelineBarrier(stagingcommandbuffer.at(0), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
		                     0, nullptr, 0, nullptr, 1, &stagingbuffershaderbarrier);

		if (vkEndCommandBuffer(stagingcommandbuffer.at(0)) != VK_SUCCESS) {
			logger::log(0, "crashed in texture at vkEndCommandBuffer");
			return false;
		}

		VkSubmitInfo submitinfo{};
		submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitinfo.pWaitDstStageMask = nullptr;
		submitinfo.waitSemaphoreCount = 0;
		submitinfo.pWaitSemaphores = nullptr;
		submitinfo.signalSemaphoreCount = 0;
		submitinfo.pSignalSemaphores = nullptr;
		submitinfo.commandBufferCount = 1;
		submitinfo.pCommandBuffers = stagingcommandbuffer.data();

		VkFence stagingbufferfence;

		VkFenceCreateInfo fenceinfo{};
		fenceinfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceinfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		if (vkCreateFence(rdata.vkdevice.device, &fenceinfo, nullptr, &stagingbufferfence) != VK_SUCCESS) {
			logger::log(0, "crashed in texture at vkCreateFence");
			return false;
		}
		if (vkResetFences(rdata.vkdevice.device, 1, &stagingbufferfence) != VK_SUCCESS) {
			logger::log(0, "crashed in texture at vkResetFences");
			return false;
		}

		rdata.mtx2->lock();
		if (vkQueueSubmit(rdata.graphicsQ, 1, &submitinfo, stagingbufferfence) != VK_SUCCESS) {
			logger::log(0, "crashed in texture at vkQueueSubmit");
			return false;
		}

		rdata.mtx2->unlock();

		if (vkWaitForFences(rdata.vkdevice.device, 1, &stagingbufferfence, VK_TRUE, INT64_MAX) != VK_SUCCESS) {
			logger::log(0, "crashed in texture at vkWaitForFences");
			return false;
		}

		vkDestroyFence(rdata.vkdevice.device, stagingbufferfence, nullptr);
		commandbuffer::destroy(rdata, rdata.cpools_graphics.at(1), stagingcommandbuffer);
		vmaDestroyBuffer(rdata.alloc, stagingbuffer, stagingbufferalloc);

		VkImageViewCreateInfo texviewinfo{};
		texviewinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		texviewinfo.image = texdata[i].img;
		texviewinfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		texviewinfo.format = VK_FORMAT_R8G8B8A8_SRGB;
		texviewinfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		texviewinfo.subresourceRange.baseMipLevel = 0;
		texviewinfo.subresourceRange.levelCount = 1;
		texviewinfo.subresourceRange.baseArrayLayer = 0;
		texviewinfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(rdata.vkdevice.device, &texviewinfo, nullptr, &texdata[i].imgview) != VK_SUCCESS) {
			logger::log(0, "crashed in texture at vkCreateImageView");
			return false;
		}

		VkSamplerCreateInfo texsamplerinfo{};
		texsamplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		texsamplerinfo.magFilter = VK_FILTER_LINEAR;
		texsamplerinfo.minFilter = VK_FILTER_LINEAR;
		texsamplerinfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		texsamplerinfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		texsamplerinfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		texsamplerinfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		texsamplerinfo.unnormalizedCoordinates = VK_FALSE;
		texsamplerinfo.compareEnable = VK_FALSE;
		texsamplerinfo.compareOp = VK_COMPARE_OP_ALWAYS;
		texsamplerinfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		texsamplerinfo.mipLodBias = 0.0f;
		texsamplerinfo.minLod = 0.0f;
		texsamplerinfo.maxLod = 0.0f;
		texsamplerinfo.anisotropyEnable = VK_FALSE;
		texsamplerinfo.maxAnisotropy = 1.0f;

		if (vkCreateSampler(rdata.vkdevice.device, &texsamplerinfo, nullptr, &texdata[i].imgsampler) != VK_SUCCESS) {
			logger::log(0, "crashed in texture at vkCreateSampler");
			return false;
		}
	}

	return true;
}
bool vktexture::loadtexset(rvk &rdata, std::vector<texdata> &texdata, VkDescriptorSetLayout &dlayout,VkDescriptorSet &dset,
                                  fastgltf::Asset &mmodel) {

	std::vector<VkDescriptorImageInfo> descriptorimginfo;
	descriptorimginfo.reserve(mmodel.images.size());
	descriptorimginfo.resize(mmodel.images.size());

	for (size_t i{0}; i < mmodel.images.size(); i++) {

		descriptorimginfo[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		descriptorimginfo[i].imageView = texdata[i].imgview;
		descriptorimginfo[i].sampler = texdata[i].imgsampler;
	}


	VkDescriptorSetAllocateInfo descallocinfo{};
	descallocinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descallocinfo.descriptorPool = rdata.dpools[rvk::idxinitpool];
	descallocinfo.descriptorSetCount = 1;
	descallocinfo.pSetLayouts = &dlayout;

	if (vkAllocateDescriptorSets(rdata.vkdevice.device, &descallocinfo, &dset) != VK_SUCCESS) {
		logger::log(0, "crashed in texture at vkAllocateDescriptorSets");
		return false;
	}

	VkWriteDescriptorSet writedescset{};
	writedescset.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writedescset.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writedescset.dstSet = dset;
	writedescset.dstArrayElement = 0;
	writedescset.dstBinding = 0;
	// writedescset.pBufferInfo = 0;
	writedescset.descriptorCount = mmodel.images.size();
	writedescset.pImageInfo = descriptorimginfo.data();
	vkUpdateDescriptorSets(rdata.vkdevice.device, 1, &writedescset, 0, nullptr);

	return true;
}

void vktexture::cleanup(rvk &rdata, texdata &texdata) {
	vkDestroySampler(rdata.vkdevice.device, texdata.imgsampler, nullptr);
	vkDestroyImageView(rdata.vkdevice.device, texdata.imgview, nullptr);
	vmaDestroyImage(rdata.alloc, texdata.img, texdata.alloc);
}
bool vktexture::createlayout(rvk &core){
	
	VkDescriptorSetLayoutBinding texturebind;
	texturebind.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	texturebind.binding = 0;
	texturebind.descriptorCount = 24;//max ~24 images per gltf file
	texturebind.pImmutableSamplers = nullptr;
	texturebind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo texcreateinfo{};
	texcreateinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	texcreateinfo.bindingCount = 1;
	texcreateinfo.pBindings = &texturebind;

	if (vkCreateDescriptorSetLayout(core.vkdevice.device, &texcreateinfo, nullptr, rvk::texlayout.get()) !=
	        VK_SUCCESS) {
		std::cout << "texture set layout crashed" << std::endl;
		return false;
	}
	return true;

}

// void vktexture::cleanuppls(rvk &rdata, texdataset &texdatapls) {
// 	vkDestroyDescriptorSetLayout(rdata.vkdevice.device, texdatapls.dlayout, nullptr);
// }
